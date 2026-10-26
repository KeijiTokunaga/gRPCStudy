# ファイル別ガイド

このリポジトリは、C++のgRPCサーバーとクライアントを別コンテナで動かし、
共有ボリューム上のUDS（Unix Domain Socket）で通信する学習環境です。

## 全体の関係と読む順番

```text
proto/greeter.proto                 RPCとメッセージの契約
        ↓ protoc + grpc_cpp_plugin（CMakeが実行）
greeter.pb.* / greeter.grpc.pb.*     ビルド時に生成
        ↓ リンク
src/server.cpp / src/client.cpp     アプリケーションの実装
        ↓ Dockerfileでコンパイル
server / client                    コンテナ内の実行ファイル
        ↓ compose.yamlで別々に起動
/sockets/greeter.sock               共有ボリューム上の通信口
        ↓ tests/smoke.shで検証
.github/workflows/test.yml          GitHub上でも同じ検証を実行
```

初めて読む場合は **proto → server → client → CMake → Dockerfile → Compose → テスト** の順がおすすめです。

## ファイル一覧

| ファイル | 役割 |
| --- | --- |
| [proto/greeter.proto](../proto/greeter.proto) | RPC・リクエスト・レスポンスの定義 |
| [src/server.cpp](../src/server.cpp) | UDSで待ち受け、挨拶を返すサーバー |
| [src/client.cpp](../src/client.cpp) | RPCを呼び、返答を検証するクライアント |
| [CMakeLists.txt](../CMakeLists.txt) | コード生成・コンパイル・リンクの手順 |
| [Dockerfile](../Dockerfile) | ビルド用と実行用のコンテナイメージ定義 |
| [compose.yaml](../compose.yaml) | コンテナ起動・共有ボリューム・ネットワークの設定 |
| [tests/smoke.sh](../tests/smoke.sh) | 正常系・異常系の通信テスト |
| [.github/workflows/test.yml](../.github/workflows/test.yml) | GitHub Actionsの自動テスト |
| [.dockerignore](../.dockerignore) | Dockerへ送るビルド用ファイルの制限 |
| [.gitignore](../.gitignore) | Gitで管理しないファイルの指定 |
| [README.md](../README.md) | 起動・操作・テストの入口 |
| [docs/communication-and-virtualization.md](communication-and-virtualization.md) | 通信シーケンス・ソケット・仮想化の解説 |
| [docs/file-guide.md](file-guide.md) | 本書。各ファイルの役割と変更時の注意点 |

## proto/greeter.proto — 通信の契約

`proto3` 形式で、`study` パッケージ内に次を定義します。

- `Greeter`：RPCサービス。
- `SayHello`：`HelloRequest` を1件受け取り、`HelloReply` を1件返すUnary RPC。
- `HelloRequest.name`：送信する名前。フィールド番号は1。
- `HelloReply.message`：返答の文字列。フィールド番号は1。

C++では `study::HelloRequest` や `study::Greeter` として利用します。
フィールド番号は通信データ上の識別子です。互換性を保つ場合は、既存番号の意味を変更したり、
削除した番号を別の用途へ再利用したりしないでください。

## src/server.cpp — RPCを処理する

`Greeter` クラスが、生成された `study::Greeter::Service` を継承して `SayHello` を実装します。

1. 名前が空なら `INVALID_ARGUMENT`（コード3）を返します。
2. 名前があれば `Hello, <名前>!` を返答に設定し、処理した名前をログへ出します。
3. `grpc::Status::OK` でRPC成功を伝えます。

`main()` はサービスを登録し、`unix:/sockets/greeter.sock` で待ち受けます。
`AddListeningPort` というAPI名ですが、ここで指定しているのはTCPポートではなくUDSです。
起動に失敗した場合は終了コード1で終了し、成功時は `server->Wait()` で処理を続けます。

`InsecureServerCredentials()` を使っており、TLS・RPC認証はありません。
この教材では共有ディレクトリの権限で接続できるプロセスを制限しています。

## src/client.cpp — RPCを呼び出す

引数は `client [name] [target]` です。省略時は名前が `Docker`、
接続先が `unix:/sockets/greeter.sock` になります。

1. `CreateChannel` で接続先を指定し、`NewStub` でRPC呼び出し口を作ります。
2. リクエストに名前を設定します。
3. `ClientContext` に `wait_for_ready` と10秒の期限を設定します。
4. `stub->SayHello()` を同期的に呼び出します。
5. RPCの成否と返答文字列を確認し、成功時は返答を表示します。

`wait_for_ready` はサーバーの準備が間に合わない場合に期限内で接続準備を待つための設定です。
10秒は接続準備だけではなく、RPC全体に対する期限です。
引数過多・RPC失敗・期待と異なる返答は終了コード1、成功は0です。
返答の期待値をコード内で確認するため、クライアント自体が簡単な通信テストも兼ねています。

## CMakeLists.txt — C++の生成とビルド

CMake 3.16以上とC++17を指定し、インストール済みのProtobufとgRPCを検索します。

- `add_custom_command`：`protoc` と `grpc_cpp_plugin` で `.proto` からC++を生成。
- `greeter_proto`：生成されたメッセージとRPCコードをまとめるライブラリ。
- `target_include_directories`：生成ヘッダーの場所を利用側へ公開。
- `target_link_libraries`：Protobuf・gRPCと生成コードをアプリへリンク。
- `foreach(app server client)`：2つの実行ファイルを同じ設定でビルド。
- `-Wall -Wextra -Wpedantic`：アプリのコンパイル時の警告を有効化。

`.proto` が更新されると、依存関係に従って生成コードも再生成されます。

## Dockerfile — ビルド環境と実行環境

どちらのステージも `debian:bookworm-slim` を使うマルチステージビルドです。

**buildステージ**ではコンパイラ、CMake、gRPC・Protobufの開発パッケージと生成ツールを入れ、
`CMakeLists.txt`・`proto/`・`src/` をコピーしてReleaseビルドします。
`--parallel 2` はビルドの並列数です。

**実行ステージ**では実行時ライブラリとビルド済みの `server`・`client` だけを配置します。
コンパイラは実行イメージに含めません。
`/sockets` をUID/GID 10001の所有、モード0700で作り、同じ非rootユーザーで実行します。
新規の名前付きボリュームには、このディレクトリの所有権・権限が引き継がれます。

`CMD ["server"]` が標準の起動コマンドです。Composeの `command` でクライアントにも切り替えます。

## compose.yaml — 2つのコンテナを接続する

`server` と `client` は同じ `grpc-study:local` イメージを使います。
イメージは `server` の `build: .` でビルドし、各サービスの `command` で実行ファイルを指定します。
`client` の `pull_policy: never` はローカルのビルド済みイメージを使うための設定です。
初回はREADMEにある `--build` 付きコマンドでビルドしてください。

両コンテナは名前付きボリューム `sockets` を `/sockets` にマウントします。
サーバーが作ったソケットをクライアント側からも同じパスで参照できます。
クライアント側は `:ro`（読み取り専用）ですが、LinuxではUDSへの接続・送受信ができます。

- `network_mode: none`：コンテナ間のIP通信を使わずUDSで動く構成。
- `depends_on`：サーバーを先に起動。ただし待ち受け準備完了までは保証しないため、クライアント側でも待機。
- `init: true`：サーバーコンテナに小さなinitプロセスを入れ、停止シグナルの転送などを処理。

`docker compose down` はコンテナを削除しても名前付きボリュームを残します。
`docker compose down -v` は本教材のボリューム内データも削除します。

## tests/smoke.sh — 実際のコンテナ間通信を検証する

`sh tests/smoke.sh` で実行します。プロジェクトルートへ移動してから次を確認します。

1. イメージをビルドし、デフォルトの挨拶RPCが終了コード0で成功すること。
2. 同じボリュームを使ってサーバーを再起動し、`Restart` への挨拶が成功すること。
3. `/sockets/greeter.sock` がソケットであること（`test -S`）。
4. 空の名前でRPCが失敗し、コード3と所定のメッセージが出ること。
5. 存在しないソケットへ接続すると期限切れで失敗し、コード4が出ること。

`set -eu` で未定義変数や予期しないコマンド失敗を検出し、
`trap` により成功・失敗のいずれでもスクリプト終了時に `docker compose down` を実行します。
異常系では、失敗すべきコマンドが成功してしまった場合もテスト失敗とします。
専用の外部テストライブラリは使っていません。

## .github/workflows/test.yml — GitHub上の自動検証

pushとpull requestをきっかけに、`ubuntu-latest` 上でリポジトリをcheckoutし、
`sh tests/smoke.sh` を実行します。ジョブの制限時間は15分です。
ローカルと同じスクリプトを使うので、検証内容を二重に管理しません。

## .dockerignore — Dockerへ送るファイルを絞る

最初の `*` でファイルを原則除外し、`!` で `CMakeLists.txt`・`proto/`・`src/` を許可します。
README、Git履歴、ローカル生成物などはアプリのビルドコンテキストに含めません。
ビルドに必要な新しいディレクトリを追加する場合は、この許可設定とDockerfileの `COPY` も確認します。
Dockerfile自体はDockerがビルド定義として読み込みます。

## .gitignore — Gitの管理対象を絞る

`build/` とmacOSの `.DS_Store` を除外します。
生成コードやローカルのビルド結果を誤ってコミットしないための設定です。
`.dockerignore` とは目的が異なり、Dockerへ送るファイルを決める設定ではありません。

## README.mdとdocs/ — 操作と解説

- `README.md`：必要なツール、起動コマンド、名前を変えて試す操作、テスト方法、制約の入口。
- `docs/communication-and-virtualization.md`：シーケンス図、ログの読み方、UDS・VSOCK・TCPの比較、VM・ホストの構成例。
- `docs/file-guide.md`：本書。実ファイルと役割を対応付け、実装を読み進めるためのガイド。

## Git管理しない生成物と実行時データ

| 生成物 | 生成するもの | 内容・扱い |
| --- | --- | --- |
| `build/greeter.pb.h`・`.cc` | protoc | メッセージクラスとシリアライズ処理 |
| `build/greeter.grpc.pb.h`・`.cc` | grpc_cpp_plugin | サーバーのServiceとクライアントのStub |
| `build/server`・`build/client` | CMakeとコンパイラ | C++実行ファイル。実行イメージでは `/usr/local/bin/` に配置 |
| `/sockets/greeter.sock` | サーバー起動 | 共有ボリューム上の通信口。通信履歴を保存するファイルではない |

Dockerビルド中の `build/` はビルドステージ内の `/src/build/` です。
生成C++は直接編集せず、`.proto` と実装を編集して再ビルドします。

## 変更するときの対応関係

| 変更したいこと | 一緒に確認するファイル |
| --- | --- |
| 返答文を変える | `src/server.cpp` の返答と `src/client.cpp` の期待値 |
| RPCやフィールドを変える | `.proto`、server、client、テスト、説明文書 |
| ソケットパスを変える | server、clientのデフォルト、Composeの引数、テスト、説明文書。ディレクトリも変えるならDockerfileとボリューム設定 |
| 新しいソースを追加する | CMake、必要に応じてDockerfileと `.dockerignore` |
| 実行ユーザーを変える | DockerfileのUSERとディレクトリ所有権。既存ボリュームの権限は自動更新されない点にも注意 |

本書は現在のソースコードの説明です。VSOCK、TLS、RPC認証、ストリーミングは実装していません。
