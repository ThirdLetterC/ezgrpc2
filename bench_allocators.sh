#!/usr/bin/env bash
set -e

PORT=19009
GHZ="$HOME/go/bin/ghz"
PROTO="examples/python_client_c_server/helloworld.proto"
CALL="helloworld.Greeter.SayHello"
DATA='{"name":"benchmark_user"}'

run_test() {
  local name="$1"
  local allocator="$2"
  local build_dir="build-bench-$allocator"

  echo "============================================================"
  echo " Benchmarking Allocator: $name"
  echo " CMake allocator: $allocator"
  echo "============================================================"

  cmake -S . -B "$build_dir" -DCMAKE_BUILD_TYPE=Release \
    -DEZGRPC2_ALLOCATOR="$allocator" \
    -DEZGRPC2_BUILD_TESTS=OFF \
    -DEZGRPC2_BUILD_INTEGRATION_TESTS=OFF >/dev/null
  cmake --build "$build_dir" --target example_grpc_server --parallel >/dev/null 2>&1

  "$build_dir/server.bin" $PORT 1 >/dev/null 2>&1 &
  SERVER_PID=$!
  sleep 1

  # Warmup
  echo "Warming up..."
  $GHZ --insecure \
    --proto="$PROTO" \
    --call="$CALL" \
    -d "$DATA" \
    -c 20 -n 5000 127.0.0.1:$PORT >/dev/null 2>&1

  echo "--- Benchmark: Concurrency 20, 50,000 Requests ---"
  $GHZ --insecure \
    --proto="$PROTO" \
    --call="$CALL" \
    -d "$DATA" \
    -c 20 -n 50000 127.0.0.1:$PORT

  echo "--- Benchmark: Concurrency 50, 100,000 Requests ---"
  $GHZ --insecure \
    --proto="$PROTO" \
    --call="$CALL" \
    -d "$DATA" \
    -c 50 -n 100000 127.0.0.1:$PORT

  echo "--- Benchmark: Concurrency 100, 100,000 Requests ---"
  $GHZ --insecure \
    --proto="$PROTO" \
    --call="$CALL" \
    -d "$DATA" \
    -c 100 -n 100000 127.0.0.1:$PORT

  kill $SERVER_PID 2>/dev/null || true
  wait $SERVER_PID 2>/dev/null || true
  sleep 1
}

# 1. Standard libc allocator
run_test "Standard libc (glibc)" "default"

# 2. Microsoft mimalloc
run_test "Microsoft mimalloc" "mimalloc"

# 3. Jemalloc
run_test "jemalloc" "jemalloc"

echo "============================================================"
echo " Benchmarks Completed Successfully!"
echo "============================================================"
