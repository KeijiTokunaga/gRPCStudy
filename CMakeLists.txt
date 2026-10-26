# 全体の流れ: Dockerfile → このCMake設定 → protoからC++生成 → server/clientをビルド。
# 生成コードは通信の共通部分、src/は挨拶の処理を担当し、Composeが実行ファイルを別コンテナで起動する。
# protoの変更は再生成に反映される。生成ファイルは直接編集せず、protoまたはsrcを変更する。
# 1. 言語規格をC++17に統一し、Dockerfileでインストールした依存ライブラリを検索する。
cmake_minimum_required(VERSION 3.16)
project(grpc_study LANGUAGES CXX)
set(CMAKE_CXX_STANDARD 17)
set(CMAKE_CXX_STANDARD_REQUIRED ON)
find_package(Protobuf REQUIRED)
find_package(gRPC CONFIG REQUIRED)

# 2. メッセージ用とRPC用のC++をビルドディレクトリへ生成する。
# grpc_cpp_pluginはService/Stubを生成するprotocのプラグイン。DEPENDSでproto更新時に再生成する。
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
# 3. 生成コードを共通ライブラリにまとめ、ヘッダーの場所と依存ライブラリも利用側へ伝える。
add_library(greeter_proto greeter.pb.cc greeter.grpc.pb.cc)
target_include_directories(greeter_proto PUBLIC ${CMAKE_CURRENT_BINARY_DIR})
target_link_libraries(greeter_proto PUBLIC protobuf::libprotobuf gRPC::grpc++)
# 4. server/clientを共通ライブラリへリンクする。警告を有効にし、実装時のミスに気付きやすくする。
foreach(app server client)
  add_executable(${app} src/${app}.cpp)
  target_link_libraries(${app} PRIVATE greeter_proto)
  target_compile_options(${app} PRIVATE -Wall -Wextra -Wpedantic)
endforeach()
