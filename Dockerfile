# 全体の流れ: ソースとprotoを取り込む → CMakeで生成・ビルド → 実行用イメージへコピー。
# server/client共通のイメージを作り、compose.yamlのcommandで起動する実行ファイルを選ぶ。
# 実行時は共有ボリューム上の /sockets/greeter.sock を使い、コンテナ間のIP通信は行わない。
# 1. ビルド専用ステージ。コンパイラ、開発ヘッダー、protocとgRPCプラグインを準備する。
FROM debian:bookworm-slim AS build
RUN apt-get update && apt-get install -y --no-install-recommends \
    build-essential cmake libgrpc++-dev protobuf-compiler-grpc libprotobuf-dev \
    && rm -rf /var/lib/apt/lists/*
# 2. .dockerignoreで許可された入力を配置。READMEやGit履歴はビルドに取り込まない。
WORKDIR /src
COPY CMakeLists.txt ./
COPY proto ./proto
COPY src ./src
# 3. CMakeがprotoのC++生成からリンクまで実行し、/src/buildに2つの実行ファイルを作る。
RUN cmake -S . -B build -DCMAKE_BUILD_TYPE=Release \
    && cmake --build build --parallel 2

# 4. 実行専用ステージ。必要な共有ライブラリと実行ファイルだけを残し、コンパイラは含めない。
FROM debian:bookworm-slim
RUN apt-get update && apt-get install -y --no-install-recommends libgrpc++1.51 libprotobuf32 \
    && rm -rf /var/lib/apt/lists/*
COPY --from=build /src/build/server /src/build/client /usr/local/bin/
# 5. 新規ボリュームに引き継ぐUDS用ディレクトリを準備する。0700は所有者だけがアクセス可能。
# 両コンテナを同じUID/GIDで動かし、rootで動かさずに共有UDSへアクセスできるようにする。
# 既存ボリュームの所有権・権限は、このイメージの再ビルドだけでは更新されない。
RUN install -d -o 10001 -g 10001 -m 0700 /sockets
USER 10001:10001
# 6. 標準ではserverを実行。Composeのclientサービスではcommandでclientに上書きする。
CMD ["server"]
