# gRPCStudy

Dockerコンテナ同士でUDS（Unix Domain Socket）経由のgRPC通信を試す、C++17の小さな学習環境です。
サーバーへ名前を送り、`Hello, Docker!` を受け取ります。

```text
client コンテナ                       server コンテナ
C++クライアント ── SayHello(name) ──▶ C++サーバー
               ◀── HelloReply ──────
         共有ボリューム: /sockets/greeter.sock（UDS）
```

## 学習ドキュメント

[各ファイルの役割・処理・変更時の対応関係](docs/file-guide.md)

[通信シーケンス・UDS／VSOCK／TCP・VMとホストの構成例](docs/communication-and-virtualization.md)

## 必要なもの

- Docker Engine / Docker Desktop（起動済み）
- Docker Compose v2以降（`docker compose`）
- 初回ビルド用のインターネット接続

ホスト側にC++、CMake、gRPCをインストールする必要はありません。
Debian 12のパッケージでコンパイラ・gRPC・Protocol Buffersを揃え、
ビルド用と実行用のイメージを分けています。Apple Siliconでもネイティブにビルドできます。

## まず1往復してみる

```sh
git clone https://github.com/KeijiTokunaga/gRPCStudy.git
cd gRPCStudy
docker compose up --build --abort-on-container-exit --exit-code-from client
```

初回は依存パッケージをダウンロードしてC++をビルドします。
クライアントに `Hello, Docker!` が表示され、終了コード0で終了すれば成功です。
サーバーはクライアント終了後に停止します。

```sh
docker compose down
```

## サーバーを動かしたまま試す

```sh
docker compose up --build -d server
docker compose run --rm client client Taro
docker compose run --rm client client "gRPC learner"
docker compose logs server
docker compose down
```

出力例: `Hello, Taro!`

`docker compose run` の最初の `client` はサービス名、次の `client` は
コンテナ内のC++実行ファイル名です。実行ファイルの引数は `[名前] [接続先target]`。
省略時は `Docker` と `unix:/sockets/greeter.sock` を使います。

## エラーとテスト

空の名前はサーバーが `INVALID_ARGUMENT`（コード3）で拒否します。
接続できない場合は10秒の期限後に `DEADLINE_EXCEEDED`（コード4）で失敗します。
クライアントはRPC失敗や期待と異なる応答を終了コード1で通知します。

```sh
docker compose up --build -d server
docker compose run --rm client client ""
docker compose run --rm client client Docker unix:/sockets/missing.sock
docker compose down

# 正常応答・空入力・接続期限を自動検証。終了時にコンテナも削除。
sh tests/smoke.sh
```

両コンテナが名前付きボリューム `sockets` を `/sockets` にマウントし、同じUDSに接続します。
クライアント側は読み取り専用マウントですが、LinuxではUDSへの接続・送受信が可能です。
両方とも `network_mode: none` なので、コンテナ間のIPネットワークは使いません。
Dockerfileで共有ディレクトリをUID/GID 10001の所有・モード0700にし、両プロセスも同じ非root UIDで動かします。
新規ボリュームにはイメージ側ディレクトリの所有権・権限が引き継がれます。
MacでもソケットはDockerのLinux環境内に置かれ、macOS側のパス共有を必要としません。

`docker compose down` ではソケット用ボリュームを保持します。
完全に片付ける場合は `docker compose down -v` で削除できます（この教材のボリューム内データも削除されます）。
Composeの起動順指定だけではサーバーの準備完了を保証しないため、
クライアントは期限内で接続準備を待つ `wait_for_ready` を使います。
GitHub Actionsでも同じテストを実行します。

## ファイルと学習の流れ

| ファイル | 役割 |
| --- | --- |
| `proto/greeter.proto` | RPCメソッドと送受信メッセージの契約 |
| `src/server.cpp` | 生成されたServiceを継承し、RPCを実装 |
| `src/client.cpp` | 生成されたStubでRPCを呼び、応答とエラーを確認 |
| `CMakeLists.txt` | protocでC++を自動生成してビルド |
| `Dockerfile` | C++のビルド環境と非rootの実行環境 |
| `compose.yaml` | サーバー・クライアントとUDS共有ボリューム |
| `tests/smoke.sh` | 別コンテナ間の通信とエラー処理のテスト |

まずproto定義→サーバー実装→クライアント呼び出しの順に読むと追いやすい構成です。
例えば返答文を変える場合は、サーバーの返答とクライアントの期待値を合わせて変更し、
`sh tests/smoke.sh` で再ビルド・検証してください。
生成コードはビルド時に作るため、Gitには含めません。

## この教材の範囲

Unary RPC（リクエスト1件・応答1件）のみです。認証・TLS・ストリーミングは含みません。
TCPポートを待ち受けず、権限を制限したUDS経由で平文通信します。
本番環境に使う際はソケットへのアクセス制御や認証などを別途設計してください。
Debianのパッケージ更新を取り込むため、依存バージョンは完全固定していません。

## 参考

- [gRPC C++ Quick start](https://grpc.io/docs/languages/cpp/quickstart/)
- [Docker Composeの起動順制御](https://docs.docker.com/compose/how-tos/startup-order/)
- [gRPCの接続先URI（Unix domain sockets）](https://github.com/grpc/grpc/blob/master/doc/naming.md)
- [Dockerのボリュームと初期データコピー](https://docs.docker.com/engine/storage/volumes/)
