# gRPCStudy

Dockerコンテナ同士でgRPC通信を試す、C++17の小さな学習環境です。
サーバーへ名前を送り、`Hello, Docker!` を受け取ります。

```text
client コンテナ                       server コンテナ
C++クライアント ── SayHello(name) ──▶ C++サーバー :50051
               ◀── HelloReply ──────
         Docker Composeのネットワーク（DNS名: server）
```

## 学習ドキュメント

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
コンテナ内のC++実行ファイル名です。実行ファイルの引数は `[名前] [接続先host:port]`。
省略時は `Docker` と `server:50051` を使います。

## エラーとテスト

空の名前はサーバーが `INVALID_ARGUMENT`（コード3）で拒否します。
接続できない場合は10秒の期限後に `DEADLINE_EXCEEDED`（コード4）で失敗します。
クライアントはRPC失敗や期待と異なる応答を終了コード1で通知します。

```sh
docker compose up --build -d server
docker compose run --rm client client ""
docker compose run --rm client client Docker 127.0.0.1:1
docker compose down

# 正常応答・空入力・接続期限を自動検証。終了時にコンテナも削除。
sh tests/smoke.sh
```

`127.0.0.1` はクライアントコンテナ自身です。別コンテナのサーバーには
Composeのサービス名 `server` で接続します。
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
| `compose.yaml` | サーバー・クライアントとネットワーク |
| `tests/smoke.sh` | 別コンテナ間の通信とエラー処理のテスト |

まずproto定義→サーバー実装→クライアント呼び出しの順に読むと追いやすい構成です。
例えば返答文を変える場合は、サーバーの返答とクライアントの期待値を合わせて変更し、
`sh tests/smoke.sh` で再ビルド・検証してください。
生成コードはビルド時に作るため、Gitには含めません。

## この教材の範囲

Unary RPC（リクエスト1件・応答1件）のみです。認証・TLS・ストリーミングは含みません。
ポートをホストへ公開せず、Composeネットワーク内で平文通信します。
本番環境に使う際は認証・TLSなどを別途設計してください。
Debianのパッケージ更新を取り込むため、依存バージョンは完全固定していません。

## 参考

- [gRPC C++ Quick start](https://grpc.io/docs/languages/cpp/quickstart/)
- [Docker Composeの起動順制御](https://docs.docker.com/compose/how-tos/startup-order/)
