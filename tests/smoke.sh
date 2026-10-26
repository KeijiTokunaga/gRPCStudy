#!/bin/sh
set -eu
cd "$(dirname "$0")/.."
trap 'docker compose down' EXIT
# A successful exit also verifies the exact reply in the C++ client.
docker compose up --build --abort-on-container-exit --exit-code-from client
docker compose up -d server
output=$(docker compose run --rm client client "" 2>&1) && {
  echo "FAIL: empty name was accepted"
  exit 1
}
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -F 'RPC failed (3): name must not be empty'
# No server is listening here: the client must fail after its deadline.
output=$(docker compose run --rm client client Docker 127.0.0.1:1 2>&1) && {
  echo "FAIL: unreachable server succeeded"
  exit 1
}
printf '%s\n' "$output"
printf '%s\n' "$output" | grep -F 'RPC failed (4):'
echo "PASS: greeting, invalid input, and deadline"
