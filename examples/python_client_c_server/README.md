# Python gRPC Client & C ezgrpc2 Server Example

This example demonstrates an end-to-end gRPC service where:
- The **Server** is written in **C** using `ezgrpc2` and `protobuf-c`.
- The **Client** is written in **Python** using standard `grpcio`.

## Service Definition

The service is defined in `helloworld.proto`:

```protobuf
syntax = "proto3";

package helloworld;

service Greeter {
  rpc SayHello (HelloRequest) returns (HelloReply);
  rpc SayHelloAgain (HelloRequest) returns (HelloReply);
}

message HelloRequest {
  string name = 1;
}

message HelloReply {
  string message = 1;
}
```

## Prerequisites

- `protobuf-c-compiler` and `libprotobuf-c-dev` (or installed via `~/.local`)
- Python 3 with `grpcio` and `grpcio-tools`:
  ```bash
  pip install grpcio grpcio-tools protobuf
  ```

## Building and Running

1. **Configure and build** (generates protobuf bindings and compiles the C server):
   ```bash
   cmake -S ../.. -B ../../build -DCMAKE_BUILD_TYPE=Release
   cmake --build ../../build --target example_grpc_server --parallel
   ```

2. **Automated End-to-End Test**:
   ```bash
   ctest --test-dir ../../build -R integration_grpc_cleartext --output-on-failure
   ```
   This starts the C server in the background, executes the Python client calls, validates the responses, and shuts down the server.

3. **Running Manually**:
   - Terminal 1 (Start C server):
     ```bash
     ../../build/server.bin 19009
     ```
   - Terminal 2 (Run Python client):
     ```bash
     python3 -m grpc_tools.protoc -I. --python_out=../../build/generated/protobuf-python --grpc_python_out=../../build/generated/protobuf-python helloworld.proto
     PYTHONPATH=../../build/generated/protobuf-python python3 client.py localhost:19009
     ```
