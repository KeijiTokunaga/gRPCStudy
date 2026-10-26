#include <iostream>
#include <grpcpp/grpcpp.h>
#include "greeter.grpc.pb.h"

class Greeter final : public study::Greeter::Service {
  grpc::Status SayHello(grpc::ServerContext*, const study::HelloRequest* request,
                        study::HelloReply* reply) override {
    if (request->name().empty()) {
      return {grpc::StatusCode::INVALID_ARGUMENT, "name must not be empty"};
    }
    reply->set_message("Hello, " + request->name() + "!");
    std::cout << "SayHello: " << request->name() << std::endl;
    return grpc::Status::OK;
  }
};

int main() {
  Greeter service;
  grpc::ServerBuilder builder;
  // shortcut: plaintext for a local Docker lab; add TLS before using across hosts.
  builder.AddListeningPort("0.0.0.0:50051", grpc::InsecureServerCredentials());
  builder.RegisterService(&service);
  auto server = builder.BuildAndStart();
  if (!server) {
    std::cerr << "Failed to start server\n";
    return 1;
  }
  std::cout << "Listening on 0.0.0.0:50051" << std::endl;
  server->Wait();
}
