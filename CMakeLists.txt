cmake_minimum_required(VERSION 3.16)
project(grpc_study LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

set(PROTO "${CMAKE_CURRENT_SOURCE_DIR}/proto/greeter.proto")
add_custom_command(
  OUTPUT greeter.pb.cc greeter.pb.h greeter.grpc.pb.cc greeter.grpc.pb.h
  COMMAND protobuf::protoc
  ARGS --proto_path=${CMAKE_CURRENT_SOURCE_DIR}/proto
       --cpp_out=${CMAKE_CURRENT_BINARY_DIR}
       --grpc_out=${CMAKE_CURRENT_BINARY_DIR}
       --plugin=protoc-gen-grpc=$<TARGET_FILE:gRPC::grpc_cpp_plugin> ${PROTO}
  DEPENDS ${PROTO}
  VERBATIM)
add_library(greeter_proto greeter.pb.cc greeter.grpc.pb.cc)
target_include_directories(greeter_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(greeter_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)
foreach(app server client)
  add_executable(${app} src/${app}.cpp)
  target_link_libraries(${app} PRIVATE greeter_proto)
  target_compile_options(${app} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()
