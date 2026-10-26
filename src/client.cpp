// 全体の流れ: 引数を読む → 共有UDSへのChannel/Stubを作る → SayHelloを呼ぶ → 応答を検証して終了。
// protoから生成したStubが送受信を担い、相手のserver.cppがリクエストを処理する。
// Composeのclientコンテナで1回実行し、終了コードはtests/smoke.shとGitHub Actionsの判定にも使う。
#include <chrono>
#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>
#include "greeter.grpc.pb.h"

int main(int argc, char** argv) {
  // 1. 任意引数は名前と接続先。名前なしはDocker、接続先なしは共有UDSを使用する。
  if (argc > 3) {
    std::cerr << "Usage: client [name] [target]\n";
    return 1;
  }
  const std::string name = argc > 1 ? argv[1] : "Docker";
  const std::string target = argc > 2 ? argv[2] : "unix:/sockets/greeter.sock";
  // 2. Channelは接続先、Stubは型付きのRPC呼び出し口。ここで通信成功が確定するわけではない。
  auto stub = study::Greeter::NewStub(
      grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));
  // 3. protoの入力・出力メッセージと、今回のRPC専用の設定を準備する。
  study::HelloRequest request;
  request.set_name(name);
  study::HelloReply reply;
  grpc::ClientContext context;
  // Composeは起動順だけを保証するため、サーバーが接続可能になるまで期限内で待つ。
  context.set_wait_for_ready(true);
  // 接続準備から返答までを含む10秒の期限。ソケットが存在しなくても無期限には待たない。
  context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));
  // 4. 同期Unary RPCを1回実行する。返答または失敗が決まるまで、この呼び出し内で待つ。
  const auto status = stub->SayHello(&context, request, &reply);
  // RPC失敗はコードと理由を表示する。空入力は3、期限切れは4。テストは非0の終了コードも確認する。
  if (!status.ok()) {
    std::cerr << "RPC failed (" << status.error_code() << "): "
              << status.error_message() << '\n';
    return 1;
  }
  // 5. 通信成功だけでなく内容も検証する。serverの返答文を変えるときは、この期待値も合わせる。
  if (reply.message() != "Hello, " + name + "!") {
    std::cerr << "Unexpected response: " << reply.message() << '\n';
    return 1;
  }
  // 正常応答を表示し、main末尾に到達して終了コード0を返す。Compose/CIではこれを成功と判定する。
  std::cout << reply.message() << '\n';
}
