// 全体の流れ: protoから生成したServiceを継承 → 共有UDSで待ち受け → 名前を検証 → 挨拶を返す。
// Dockerfileでビルドされ、Composeのserverコンテナで動く。client.cppからSayHelloが呼ばれる。
// 両コンテナは同じボリュームの /sockets を参照するため、IPアドレスもTCPポートも不要。
#include <iostream>
#include <grpcpp/grpcpp.h>
#include "greeter.grpc.pb.h"

// 1. 生成ServiceのRPCメソッドを実装する。通信のシリアライズは生成コードとgRPCが担当する。
class Greeter final : public study::Greeter::Service {
  grpc::Status SayHello(grpc::ServerContext*, const study::HelloRequest* request,
                        study::HelloReply* reply) override {
    // 不正な入力はRPCのステータスとして返す。プロセスは終了せず次のリクエストも処理できる。
    if (request->name().empty()) {
      return {grpc::StatusCode::INVALID_ARGUMENT, "name must not be empty"};
    }
    // 正常時は返信メッセージを設定し、OKを返すとgRPCがクライアントへ応答する。
    reply->set_message("Hello, " + request->name() + "!");
    std::cout << "SayHello: " << request->name() << std::endl;
    return grpc::Status::OK;
  }
};

int main() {
  // 2. RPC実装を作り、UDSの待ち受け先とサービスをServerBuilderへ登録する。
  Greeter service;
  grpc::ServerBuilder builder;
  // shortcut: 学習用のためRPC認証・TLSなし。本番利用時は認証を設計し、ここでは/socketsの権限で制限する。
  builder.AddListeningPort("unix:/sockets/greeter.sock", grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  // 3. 待ち受けを開始する。ディレクトリ権限などで起動できなければ終了コード1で通知する。
  auto server = builder.BuildAndStart();
  if (!server) {
    std::cerr << "Failed to start server\n";
    return 1;
  }
  std::cout << "Listening on unix:/sockets/greeter.sock" << std::endl;
  // 4. サーバーを待機させる。Composeのabort-on-container-exit指定時はclient終了後に停止される。
  server->Wait();
}
