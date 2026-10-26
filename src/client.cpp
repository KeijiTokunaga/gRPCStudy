#include <chrono>
#include <iostream>
#include <string>
#include <grpcpp/grpcpp.h>
#include "greeter.grpc.pb.h"

int main(int argc, char** argv) {
  if (argc > 3) {
    std::cerr << "Usage: client [name] [target]\n";
    return 1;
  }
  const std::string name = argc > 1 ? argv[1] : "Docker";
  const std::string target = argc > 2 ? argv[2] : "unix:/sockets/greeter.sock";
  auto stub = study::Greeter::NewStub(
      grpc::CreateChannel(target, grpc::InsecureChannelCredentials()));
  study::HelloRequest request;
  request.set_name(name);
  study::HelloReply reply;
  grpc::ClientContext context;
  // Compose startup order alone does not guarantee the server is ready.
  context.set_wait_for_ready(true);
  context.set_deadline(std::chrono::system_clock::now() + std::chrono::seconds(10));
  const auto status = stub->SayHello(&context, request, &reply);
  if (!status.ok()) {
    std::cerr << "RPC failed (" << status.error_code() << "): "
              << status.error_message() << '\n';
    return 1;
  }
  if (reply.message() != "Hello, " + name + "!") {
    std::cerr << "Unexpected response: " << reply.message() << '\n';
    return 1;
  }
  std::cout << reply.message() << '\n';
}
