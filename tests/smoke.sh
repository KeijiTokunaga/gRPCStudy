#!/bin/sh
# 全体の流れ: Dockerビルド → 別コンテナ間のUDS通信 → 再起動 → 入力エラー → 期限切れ → 後片付け。
# C++ clientは返答自体を検証するため、正常系では終了コード0が応答内容の検証結果にもなる。
# GitHub Actionsもこのスクリプトを使う。別ディレクトリからは、このスクリプトのパスを指定して実行する。
# 予期しない失敗と未定義変数を検出し、プロジェクトルートへ移動してCompose設定を読み込む。
set -eu
cd "$(dirname "$0")/.."
# 成功・失敗にかかわらず終了時にコンテナを削除する。再起動検証に使う名前付きボリュームは保持する。
trap 'docker compose down' EXIT
# 1. ネットワークなしでHello, Docker!を受信できることを検証。client終了後にserverも停止する。
# Compose自身の終了コードをclientに合わせ、C++側の失敗をシェル/CIへ伝える。
docker compose up --build --abort-on-container-exit --exit-code-from client
# 2. 同じボリュームを保持したままserverを再起動する。
docker compose up -d server
# 前回のソケットが残り得る状態から、再び接続して挨拶できることを確認する。
docker compose run --rm client client Restart
# 共有パスが通常ファイルではなくソケットであることも確認。-TはCI向けに疑似端末を使わない指定。
docker compose exec -T server test -S /sockets/greeter.sock
# 3. 空の名前を渡す。成功してしまったら失敗扱いにし、失敗時の出力を後で検査する。
# &&の左側の失敗は想定内なのでset -eで即終了せず、終了コードと内容の両方を検証できる。
output=$(docker compose run --rm client client "" 2>&1) && {
  echo "FAIL: empty name was accepted"
  exit 1
}
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -F 'RPC failed (3): name must not be empty'
# 4. 存在しないUDSへ接続し、10秒の期限でコード4になることを確認する。
# --rmで各試行の一時クライアントコンテナを削除し、2>&1でエラー出力も検査対象にする。
output=$(docker compose run --rm client client Docker unix:/sockets/missing.sock 2>&1) && {
  echo "FAIL: unreachable server succeeded"
  exit 1
}
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -F 'RPC failed (4):'
# 5. ここまで到達すれば全項目成功。終了時には上のtrapで後片付けする。
echo "PASS: UDS greeting without network, restart, socket file, invalid input, and deadline"
