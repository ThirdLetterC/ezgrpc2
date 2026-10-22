# EZgRPC2

> [!NOTE]
> This repository is under active development; we’re tightening the bolts and refueling the engine! The hangar doors are wide open: you are encouraged to kick the tires, poke the wires, and take it for a spin in your local environment. If you manage to break the sound barrier (or find a bug), please let us know.

**EZgRPC2** is an ultra-lightweight, high-performance gRPC server implementation in C (C23 standard). Designed around a non-blocking `poll(2)` event loop, it provides fine-grained control over streams, events, concurrency, and memory without the heavyweight overhead of traditional gRPC runtimes.

---

## Features

- **Event-Driven Architecture**: Clean, non-blocking `poll(2)` event model for incoming gRPC messages, connection lifecycles, timeouts, and stream resets.
- **TLS / SSL & ALPN**: Secure HTTP/2 communication powered by OpenSSL with native ALPN (`h2`) protocol negotiation.
- **All 4 RPC Streaming Modes**:
  - **Unary**: Standard request-response.
  - **Server Streaming**: Server sends multiple messages on a single stream.
  - **Client Streaming**: Client streams multiple messages to the server.
  - **Bidirectional Streaming**: Full-duplex concurrent streaming between client and server.
- **Protobuf-C Integration**: Optional first-class support for [`protobuf-c`](https://github.com/protobuf-c/protobuf-c) messages (`EZGRPC2_PB_UNPACK`, `ezgrpc2_pb_create_response`).
- **Interceptors / Middleware Pipeline**: Pre-call and post-call hooks for logging, tracing, metrics, and built-in Bearer authentication tokens (`ezgrpc2_interceptor_create_bearer_auth`).
- **Standard gRPC Health Checking Protocol**: Built-in support for `/grpc.health.v1.Health/Check` and status management.
- **Standard gRPC Server Reflection Protocol**: Built-in discovery support for `/grpc.reflection.v1alpha.ServerReflection/ServerReflectionInfo`.
- **Custom Metadata & Binary Headers**: RFC 4648 Base64 encoding/decoding for `*-bin` trailing and initial metadata.
- **Deadlines & Timeouts**: Automatic parsing of `grpc-timeout` headers and event dispatching (`EZGRPC2_EVENT_TIMEOUT`).
- **Transparent Compression**: RFC 1952 Gzip compression and decompression powered by `zlib`.
- **Rich Error Model**: Serializes `google.rpc.Status` with `Any` details (e.g., `ErrorInfo`) into `grpc-status-details-bin` trailing metadata.
- **Pluggable Memory Allocators**: Zero-overhead support for standard libc `malloc`, Microsoft `mimalloc`, `jemalloc`, and custom runtime allocators.

---

## Architecture

The server architecture is inspired by `poll(2)`. Instead of hidden threads or opaque callback soup, you register service paths and poll an event queue:

```c
struct ezgrpc2_event {
  union {
    ezgrpc2_event_message  message;   // Received gRPC messages on a stream
    ezgrpc2_event_dataloss dataloss;  // Frame decoding or stream error
    ezgrpc2_event_cancel   cancel;    // Stream cancelled (RST_STREAM)
    ezgrpc2_event_timeout  timeout;   // Stream deadline exceeded
  };
  ezgrpc2_event_type type;            // EVENT_CONNECT, DISCONNECT, MESSAGE, etc.
  ezgrpc2_session_uuid session_uuid;  // 128-bit session identifier
  void *userdata;                     // Per-service handler callback / context
};
```

---

## Requirements & Dependencies

- **Compiler**: GCC 11+ or Clang 14+ with C23 (`-std=c2x` / `-std=c23`) support.
- **Core Dependencies**:
  - `libnghttp2` (HTTP/2 framing)
  - `pthread` (POSIX threading)
  - `libuuid` (Linux session UUIDs)
- **Optional Feature Dependencies**:
  - `OpenSSL` (`libssl`, `libcrypto`) - for TLS / SSL encryption (auto-detected)
  - `zlib` - for Gzip message compression (auto-detected)
  - `protobuf-c` - for Protobuf-C message helpers (auto-detected)
  - `mimalloc` / `jemalloc` - high-performance alternative allocators

On Debian / Ubuntu:
```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential pkg-config libnghttp2-dev \
  zlib1g-dev libssl-dev uuid-dev \
  libprotobuf-c-dev protobuf-c-compiler
```

---

## Building

Build the static library (`libezgrpc2.a`), shared library (`libezgrpc2.so`), and example servers:
```bash
make
```

### Build Options

| Make Flag | Description |
|:---|:---|
| `make test` | Builds and runs all unit tests and integration tests |
| `make D=1` | Enables `-ggdb3` debug symbols and debug logging |
| `make T=1` | Enables trace-level HTTP/2 frame logging |
| `make MIMALLOC=1` | Links with Microsoft `mimalloc` |
| `make JEMALLOC=1` | Links with `jemalloc` |
| `make ASAN=1` | Builds with AddressSanitizer (leak and memory check) |

---

## Quickstart

### 1. Initializing and Creating a Server

```c
#include "ezgrpc2.h"

int main(void) {
  // 1. Initialize library globals
  ezgrpc2_global_init(0);

  // 2. Create server listening on IPv4 and IPv6
  uint16_t port = 19009;
  ezgrpc2_server *server = ezgrpc2_server_new(
      "0.0.0.0", port,  // IPv4 binding
      "::",      port,  // IPv6 binding
      16,               // Listen backlog
      nullptr,          // Server settings (optional)
      nullptr           // HTTP/2 settings (optional)
  );

  // 3. (Optional) Enable TLS / SSL with OpenSSL
  // ezgrpc2_server_enable_tls(server, "server.crt", "server.key");

  // 4. (Optional) Register standard Health & Reflection protocols
  ezgrpc2_health_register(server);
  ezgrpc2_reflection_register(server);

  // 5. Register your gRPC RPC path
  ezgrpc2_server_register_path(server, "/helloworld.Greeter/SayHello", &my_handler, 0, 0);

  // ... Run poll loop ...

  ezgrpc2_server_free(server);
  ezgrpc2_global_cleanup();
  return 0;
}
```

### 2. Event-Driven Request Handling

```c
ezgrpc2_events *events = ezgrpc2_events_new(nullptr);

while (running) {
  // Poll incoming frames with a 500ms timeout
  int n = ezgrpc2_server_poll(server, events, 500);
  if (n <= 0) continue;

  size_t count = 0;
  ezgrpc2_event *ev = ezgrpc2_events_read(events, &count);
  for (size_t i = 0; i < count; i++) {
    switch (ev[i].type) {
      case EZGRPC2_EVENT_MESSAGE: {
        // Pop received message
        ezgrpc2_message *msg = ezgrpc2_messages_pop(ev[i].message.messages);

        // Send a response back
        char reply_payload[] = "Hello from ezgrpc2!";
        ezgrpc2_message *res = ezgrpc2_message_new(0, reply_payload, sizeof(reply_payload));
        ezgrpc2_messages *res_list = ezgrpc2_messages_new(nullptr);
        ezgrpc2_messages_push(res_list, res);

        ezgrpc2_server_session_stream_send(server, ev[i].session_uuid, ev[i].message.stream_id, res_list);
        ezgrpc2_server_session_stream_end(server, ev[i].session_uuid, ev[i].message.stream_id, EZGRPC2_GRPC_STATUS_OK);

        ezgrpc2_messages_free(res_list);
        ezgrpc2_message_free(msg);
        break;
      }
      case EZGRPC2_EVENT_TIMEOUT:
        printf("Stream %d exceeded deadline\n", ev[i].timeout.stream_id);
        break;
      case EZGRPC2_EVENT_CANCEL:
        printf("Stream %d cancelled by client\n", ev[i].cancel.stream_id);
        break;
      default:
        break;
    }
  }
  ezgrpc2_free(ev);
}
ezgrpc2_events_free(events);
```

### 3. Protobuf-C Unpacking & Packing

When compiling with `protobuf-c` available, you can unpack requests and construct responses with zero boilerplate:

```c
#include "helloworld.pb-c.h"

// Unpack incoming request
Helloworld__HelloRequest *req = EZGRPC2_PB_UNPACK(
    Helloworld__HelloRequest, &helloworld__hello_request__descriptor, req_msg);

// Prepare protobuf response
Helloworld__HelloReply reply = HELLOWORLD__HELLO_REPLY__INIT;
reply.message = "Hello, world!";

// Pack response directly into an ezgrpc2_messages queue
ezgrpc2_messages *res_msgs = ezgrpc2_pb_create_response((ProtobufCMessage *)&reply);

// Send response and finish stream
ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, res_msgs);
ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_OK);

ezgrpc2_messages_free(res_msgs);
EZGRPC2_PB_FREE_UNPACKED(req);
```

### 4. Custom Metadata, Trailers, and Rich Errors

Send trailing metadata and rich error details (`google.rpc.Status` with `ErrorInfo`):

```c
// Return an error with trailing status details
ezgrpc2_metadata_entry trailers[1] = {
    {.key = "x-server-meta", .value = "custom-value", .key_len = 13, .value_len = 12}
};

// End stream with Rich Error Details serialized to grpc-status-details-bin:
ezgrpc2_server_session_stream_end_with_details(
    server, session_uuid, stream_id,
    EZGRPC2_GRPC_STATUS_INVALID_ARGUMENT,
    "Invalid name provided",
    trailers, 1,
    "type.googleapis.com/google.rpc.ErrorInfo",
    error_info_bytes, error_info_len
);
```

---

## Complete Example & Integration Tests

The repository includes a comprehensive, verified example under [`examples/python_client_c_server/`](examples/python_client_c_server/):

- **Server (`server.c`)**: Implements Unary, Server Streaming, Client Streaming, Bidirectional Streaming, Rich Errors, Gzip compression, Bearer auth interceptors, Health Check, and Server Reflection.
- **Client (`client.py`)**: Official Python `grpcio` client testing and validating all 9 RPC scenarios across both plaintext (`h2c`) and secure TLS channels.

Run the test suite:
```bash
# Run unit tests
make -C src/tests test

# Run cleartext Python integration tests
make -C examples/python_client_c_server test

# Run TLS Python integration tests
make -C examples/python_client_c_server test-tls

# Or run everything with one command:
make test
```

---

## Continuous Integration (CI)

Every commit and pull request is automatically tested via GitHub Actions ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) against:
- **OS Matrix**: Ubuntu 24.04 and Ubuntu 22.04.
- **Compiler Matrix**: GCC and Clang (with C23 standard compliance).
- **Allocators Matrix**: Standard libc `malloc`, Microsoft `mimalloc`, and `jemalloc`.
- **Sanitizers**: AddressSanitizer (`ASAN=1`) to guarantee memory safety and zero leaks.

---

## License

```
ezgrpc2 - A lightweight, modular gRPC server in C.
https://github.com/ThirdLetterC/ezgrpc2

Copyright (c) 2023-2026 The EZgRPC2 Authors

Redistribution and use in source and binary forms, with or without
modification, are permitted provided that the following conditions
are met:

  1. Redistributions of source code must retain the above copyright notice,
     this list of conditions and the following disclaimer.

  2. Neither the name of the copyright holder nor the names of its
     contributors may be used to endorse or promote products derived from
     this software without specific prior written permission.

THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
“AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
```

On Windows, `wepoll` is licensed under the 2-Clause BSD License:
```
wepoll - epoll for Windows
https://github.com/piscisaureus/wepoll

Copyright 2012-2020, Bert Belder <bertbelder@gmail.com>
All rights reserved.
```
