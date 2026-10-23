# EZgRPC2

> [!NOTE]
> This repository is under active development; we’re tightening the bolts and refueling the engine! The hangar doors are wide open: you are encouraged to kick the tires, poke the wires, and take it for a spin in your local environment. If you manage to break the sound barrier (or find a bug), please let us know.

**EZgRPC2** is a lightweight, high-performance gRPC server library in C23 for Linux. It is built on [libuv](https://libuv.org/) and [nghttp2](https://nghttp2.org/), and exposes a simple poll-and-handle API: no hidden threads, no code generator, no callback soup. You decide how requests are scheduled, how memory is allocated, and how many cores to use.

---

## Features

- **Poll-based API**: one call (`ezgrpc2_server_poll()`) runs the event loop and returns a batch of events (connections, messages, cancellations, deadlines) for you to handle.
- **All 4 RPC modes**: unary, server streaming, client streaming and bidirectional streaming.
- **TLS with ALPN**: HTTP/2 over TLS 1.2 and 1.3, negotiating `h2`. Seven TLS libraries are supported: OpenSSL, LibreSSL, BoringSSL, AWS-LC, wolfSSL, GnuTLS and rustls (see [TLS backends](#tls-backends)).
- **Protobuf-C helpers**: optional [`protobuf-c`](https://github.com/protobuf-c/protobuf-c) integration (`EZGRPC2_PB_UNPACK`, `ezgrpc2_pb_create_response`).
- **FlatBuffers helpers**: optional [`flatcc`](https://github.com/dvidelabs/flatcc) integration with verified, zero-copy reads (`EZGRPC2_FB_UNPACK`, `ezgrpc2_fb_create_response`).
- **Interceptors**: pre-call hooks run with `ezgrpc2_server_intercept_pre()`, post-call hooks with `ezgrpc2_server_intercept_post()`. A built-in Bearer token interceptor is available (`ezgrpc2_interceptor_create_bearer_auth`).
- **Health checking**: the standard `/grpc.health.v1.Health/Check` service with per-service status.
- **Server reflection**: `grpc.reflection.v1` and `v1alpha` `ServerReflectionInfo`.
- **Metadata**: read request headers, send initial metadata and trailers, and Base64 `*-bin` values.
- **Deadlines**: `grpc-timeout` is parsed. Expired streams are ended with `DEADLINE_EXCEEDED` and reported as `EZGRPC2_EVENT_TIMEOUT`.
- **Gzip compression**: zlib-backed compression and decompression of messages.
- **Rich error model**: `google.rpc.Status` with `Any` details (e.g. `ErrorInfo`) sent in `grpc-status-details-bin`.
- **Pluggable allocators**: libc `malloc`, `mimalloc`, `jemalloc`, or your own allocator set at runtime.

---

## Architecture

You register service paths, then poll for events and answer them:

```c
struct ezgrpc2_event {
  union {
    ezgrpc2_event_message  message;   // Received gRPC messages on a stream
    ezgrpc2_event_dataloss dataloss;  // Truncated message at end of stream
    ezgrpc2_event_cancel   cancel;    // Stream cancelled by the client (RST_STREAM)
    ezgrpc2_event_timeout  timeout;   // Stream deadline exceeded
  };
  ezgrpc2_event_type type;            // EZGRPC2_EVENT_CONNECT, _DISCONNECT, _MESSAGE, ...
  ezgrpc2_session_uuid session_uuid;  // Identifies the client connection
  void *userdata;                     // The pointer you registered with the path
};
```

### I/O model

Each `ezgrpc2_server` owns one libuv loop, driven by `ezgrpc2_server_poll()`:

- Connections are libuv TCP streams (`uv_tcp_t`). Received bytes go straight from a shared read buffer into nghttp2.
- `ezgrpc2_server_session_stream_send()`, `..._stream_end()` and similar calls only **queue** frames. At the start of the next `ezgrpc2_server_poll()`, everything queued for a connection (headers, data and trailers of all its streams) is written with a single write. If your next poll may be delayed, call `ezgrpc2_server_flush()`.
- The TLS library never touches the socket: ciphertext is passed to and from it in memory, so each batch is encrypted with a single write call and every TLS library is driven the same way.

### Using more cores

A libuv loop runs on one thread. To use more cores, create one server per thread on the same port. The listening sockets use `SO_REUSEPORT`, so the kernel spreads incoming connections across the loops. Health and reflection state is shared and thread-safe. The example server does this when started with `THREADS=N`.

A single HTTP/2 connection always stays on one loop, so multiple loops only help when clients open multiple connections.

### Performance

Measured on localhost with `h2load` sending unary `SayHello` calls to the example server built with `MIMALLOC=1`:

| Setup | Requests/s |
|:---|---:|
| 1 loop, 16 connections × 32 concurrent streams | ~490k |
| 1 loop, 100 connections × 1 stream | ~147k |
| 8 loops, 64 connections × 32 concurrent streams | ~1.8M |

Numbers depend on hardware and on the client. A single `ghz` process with its default one connection tops out well below what one loop can serve, so pass `--connections=N` to `ghz` or use `h2load`.

---

## Requirements & Dependencies

- **Platform**: Linux.
- **Compiler**: GCC 11+ or Clang 14+ with C23 support (`-std=c23`, or `-std=c2x` on older compilers).
- **Required libraries**:
  - `libuv` (event loop)
  - `libnghttp2` (HTTP/2)
  - `libuuid` (session identifiers)
  - `pthread`
- **Optional libraries** (detected automatically when installed):
  - A TLS library for TLS: OpenSSL by default, or one of the [other backends](#tls-backends)
  - `zlib` for Gzip compression
  - `protobuf-c` for the Protobuf-C helpers
  - `flatcc` for the FlatBuffers helpers (no distribution package: build it with `scripts/build-flatcc.sh`)
  - `mimalloc` or `jemalloc` as the allocator (selected at build time)

On Debian / Ubuntu:

```bash
sudo apt-get update
sudo apt-get install -y \
  build-essential pkg-config libuv1-dev libnghttp2-dev uuid-dev \
  zlib1g-dev libssl-dev libprotobuf-c-dev protobuf-c-compiler \
  libmimalloc-dev libjemalloc-dev   # optional allocators
```

The Python integration tests also need `grpcio`, `grpcio-tools` and `protobuf` (`pip install grpcio grpcio-tools protobuf`).

---

## Building

The sources are laid out as:

| Directory | Contents |
|:---|:---|
| `include/` | All headers. Applications include `ezgrpc2.h` |
| `src/` | Library sources (`.c`) and build output |
| `src/tests/` | Unit tests |
| `examples/` | Example servers and the Python integration tests |

Build the static library (`src/libezgrpc2.a`), the shared library (`src/libezgrpc2.so`) and the examples:

```bash
make
```

To build your own server against it, add `include/` to the include path and link the library with its dependencies:

```bash
cc -std=c23 -D_GNU_SOURCE -Iinclude my_server.c -o my_server \
  -Lsrc -l:libezgrpc2.a -luv -lnghttp2 -luuid -lpthread -lssl -lcrypto -lz
```

Add `-lprotobuf-c` when the Protobuf-C helpers are built, `-lflatccrt` when the FlatBuffers helpers are, and the allocator library (`-lmimalloc` or `-ljemalloc`) when one is selected.

### Build options

| Command | Description |
|:---|:---|
| `make test` | Builds and runs the unit tests and the Python integration tests (cleartext and TLS) |
| `make MIMALLOC=1` | Uses `mimalloc` |
| `make JEMALLOC=1` | Uses `jemalloc` |
| `make ALLOCATOR=custom` | Allocator set at runtime with `ezgrpc2_set_allocator()` |
| `make TLS=<backend>` | Selects the TLS library: `openssl` (default when installed), `libressl`, `boringssl`, `awslc`, `wolfssl`, `gnutls`, `rustls` or `none` |
| `make TLS_PREFIX=<dir>` | Uses a TLS library installed in `<dir>/include` and `<dir>/lib` |
| `make PROTOBUF_C=0` | Disables the Protobuf-C helpers (enabled when `protobuf-c` is found) |
| `make FLATCC=0` | Disables the FlatBuffers helpers (enabled when `flatcc` is found) |
| `make FLATCC_PREFIX=<dir>` | Uses a flatcc installed in `<dir>` (`bin/flatcc`, `include/flatcc`, `lib/libflatccrt.a`) |
| `make D=1` | Debug build: `-ggdb3`, no optimization, debug logging |
| `make T=1` | Trace logging of every HTTP/2 frame (slow) |
| `make ASAN=1` | AddressSanitizer build |
| `make format` | Formats the C sources with `clang-format` |
| `make format-check` | Fails if any C source is not formatted |
| `make clean` | Removes build output |

### TLS backends

Exactly one TLS library is compiled in, selected with `TLS=`. Run `make clean` when switching. The versions listed are the ones the test suite has been run against.

| `TLS=` | Library | How to get it | Notes |
|:---|:---|:---|:---|
| `openssl` | OpenSSL 3.0 | `libssl-dev` | Default when installed |
| `libressl` | LibreSSL 4.3 | `scripts/build-tls-backend.sh libressl <dir>` | |
| `boringssl` | BoringSSL 0.20260929.0 | `scripts/build-tls-backend.sh boringssl <dir>` | |
| `awslc` | AWS-LC 5.10 | `scripts/build-tls-backend.sh awslc <dir>` | |
| `wolfssl` | wolfSSL 5.6 | `libwolfssl-dev` | Needs ALPN support (`--enable-alpn`). Without `--enable-aesni`, AES runs in software and bulk transfers are much slower |
| `gnutls` | GnuTLS 3.8 | `libgnutls28-dev` | |
| `rustls` | rustls via [rustls-ffi](https://github.com/rustls/rustls-ffi) 0.15 | `scripts/build-tls-backend.sh rustls <dir>` (needs a Rust toolchain) | Rejects clients that offer ALPN without `h2` |
| `none` | | | TLS functions fail at runtime |

LibreSSL, BoringSSL and AWS-LC install the same `openssl/*.h` headers as OpenSSL, so they are used from their own prefix. `scripts/build-tls-backend.sh` downloads a pinned, checksum-verified release, builds it, and installs it into a directory of your choice:

```bash
scripts/build-tls-backend.sh awslc "$HOME/tls/awslc"
make clean
make TLS=awslc TLS_PREFIX="$HOME/tls/awslc"
```

The build fails if the headers it finds don't belong to the selected library, and the TLS unit test checks that the library linked in is the one requested. At runtime, `ezgrpc2_tls_backend()` and `ezgrpc2_tls_backend_version()` report the library in use.

All backends behave the same way: they load a PEM certificate chain and private key, check that the key matches the certificate, allow TLS 1.2 and 1.3 only, and select `h2` with ALPN. A client that offers no ALPN still connects. For settings ezgrpc2 doesn't expose, `ezgrpc2_tls_ctx_native()` returns the library's own context (`SSL_CTX *`, `WOLFSSL_CTX *`, `gnutls_certificate_credentials_t` or `const struct rustls_server_config *`).

---

## Quickstart

### 1. A complete server

```c
#include <stdio.h>
#include <string.h>
#include "ezgrpc2.h"

static void handle_message(ezgrpc2_server *server, ezgrpc2_event *event) {
  /* the request messages; this server ignores their content */
  ezgrpc2_message *request;
  while ((request = ezgrpc2_messages_pop(event->message.messages)) != nullptr)
    ezgrpc2_message_free(request);

  /* a unary call answers once the client has finished sending */
  if (!event->message.end_stream)
    return;

  const char reply[] = "Hello from ezgrpc2!";
  ezgrpc2_messages *response = ezgrpc2_messages_new(nullptr);
  ezgrpc2_messages_push(response, ezgrpc2_message_new(0, (void *)reply, strlen(reply)));

  /* both calls only queue frames; the next poll writes them */
  ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id,
                                     response);
  ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id,
                                    EZGRPC2_GRPC_STATUS_OK);
  ezgrpc2_messages_free(response);
}

int main(void) {
  ezgrpc2_global_init(0);

  ezgrpc2_server *server = ezgrpc2_server_new("0.0.0.0", 19009, /* IPv4 */
                                              "::", 19009,      /* IPv6 */
                                              1024,             /* listen backlog */
                                              nullptr,          /* server settings */
                                              nullptr);         /* HTTP/2 settings */
  if (server == nullptr)
    return 1;

  /* optional */
  /* ezgrpc2_server_enable_tls(server, "server.crt", "server.key"); */
  ezgrpc2_health_register(server);
  ezgrpc2_reflection_register(server);

  /* the userdata pointer comes back in every event for this path */
  static int say_hello;
  ezgrpc2_server_register_path(server, "/helloworld.Greeter/SayHello", &say_hello, 0, 0);

  ezgrpc2_events *events = ezgrpc2_events_new(nullptr);
  for (;;) {
    /* writes queued responses, then waits up to 500 ms for new events */
    if (ezgrpc2_server_poll(server, events, 500) < 0)
      break;

    /* events come in blocks; read until none are left */
    ezgrpc2_event *block;
    size_t count;
    while ((block = ezgrpc2_events_read(events, &count)) != nullptr) {
      for (size_t i = 0; i < count; i++) {
        ezgrpc2_event *event = &block[i];
        if (ezgrpc2_health_handle_event(server, event) ||
            ezgrpc2_reflection_handle_event(server, event)) {
          /* answered by the built-in services */
        } else if (event->type == EZGRPC2_EVENT_MESSAGE && event->userdata == &say_hello) {
          handle_message(server, event);
        } else if (event->type == EZGRPC2_EVENT_DATALOSS) {
          ezgrpc2_server_session_stream_end(server, event->session_uuid,
                                            event->dataloss.stream_id,
                                            EZGRPC2_GRPC_STATUS_DATA_LOSS);
        }

        /* the application owns the received messages */
        if (event->type == EZGRPC2_EVENT_MESSAGE)
          ezgrpc2_messages_free(event->message.messages);
      }
      ezgrpc2_free(block);
    }
  }

  ezgrpc2_events_free(events);
  ezgrpc2_server_free(server);
  ezgrpc2_global_cleanup();
  return 0;
}
```

Paths that are not registered are answered with `UNIMPLEMENTED` automatically and produce no event.

### 2. Protobuf-C requests and responses

With `protobuf-c` installed, unpack requests and pack responses directly:

```c
#include "helloworld.pb-c.h"

ezgrpc2_message *req_msg = ezgrpc2_messages_pop(event->message.messages);
Helloworld__HelloRequest *req = EZGRPC2_PB_UNPACK(
    Helloworld__HelloRequest, &helloworld__hello_request__descriptor, req_msg);
ezgrpc2_message_free(req_msg);

Helloworld__HelloReply reply = HELLOWORLD__HELLO_REPLY__INIT;
reply.message = "Hello, world!";
ezgrpc2_messages *res_msgs = ezgrpc2_pb_create_response((ProtobufCMessage *)&reply);

ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, res_msgs);
ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id,
                                  EZGRPC2_GRPC_STATUS_OK);

ezgrpc2_messages_free(res_msgs);
EZGRPC2_PB_FREE_UNPACKED(req);
```

### 3. FlatBuffers requests and responses

With [flatcc](https://github.com/dvidelabs/flatcc), generate C code for a schema with `flatcc -a greeter.fbs`. Requests are verified and read in place, without copying or unpacking:

```c
#include "greeter_builder.h"
#include "greeter_verifier.h"

flatcc_builder_t builder;
ezgrpc2_fb_builder_init(&builder); /* once; uses the ezgrpc2 allocator */

/* verifies the buffer and returns its root table, or nullptr if it is not a
 * valid greeter.HelloRequest. The table points into req_msg: keep it alive */
greeter_HelloRequest_table_t req = EZGRPC2_FB_UNPACK(greeter_HelloRequest, req_msg);
if (req == nullptr) {
  ezgrpc2_server_session_stream_end2(server, event->session_uuid, event->message.stream_id,
                                     EZGRPC2_GRPC_STATUS_INVALID_ARGUMENT, nullptr, nullptr, 0);
  return;
}

/* fields absent from the buffer read as nullptr (strings) or their default */
const char *name = greeter_HelloRequest_name(req);
greeter_HelloReply_start_as_root(&builder);
greeter_HelloReply_message_create_str(&builder, name != nullptr ? name : "World");
greeter_HelloReply_end_as_root(&builder);

/* copies the finished buffer into a message and resets the builder for reuse */
ezgrpc2_messages *res_msgs = ezgrpc2_fb_create_response(&builder);
ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, res_msgs);
ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id,
                                  EZGRPC2_GRPC_STATUS_OK);
ezgrpc2_messages_free(res_msgs);
```

FlatBuffers services use the usual gRPC paths (`/greeter.Greeter/SayHello` for `rpc_service Greeter` in `namespace greeter`). flatcc requires `attribute "streaming";` in schemas that mark streaming methods. Message payloads (`ezgrpc2_message.data`) are aligned like `malloc()` memory, so buffers can be read where they are received.

### 4. Metadata, trailers and rich errors

Read a request header and end the stream with custom trailers:

```c
size_t len;
const char *agent = ezgrpc2_metadata_get(server, event->session_uuid, event->message.stream_id,
                                         "user-agent", &len);

ezgrpc2_metadata_entry trailers[] = {
    {.key = "x-custom-reply", .value = "pong", .key_len = 14, .value_len = 4},
};
ezgrpc2_server_session_stream_end2(server, event->session_uuid, event->message.stream_id,
                                   EZGRPC2_GRPC_STATUS_OK, nullptr, trailers, 1);
```

Return an error with `google.rpc.Status` details, serialized into `grpc-status-details-bin`:

```c
ezgrpc2_status_detail_entry detail = {
    .type_url = "type.googleapis.com/google.rpc.ErrorInfo",
    .value = error_info_bytes, /* a serialized google.rpc.ErrorInfo */
    .value_len = error_info_len,
};

ezgrpc2_server_session_stream_end_with_status_details(
    server, event->session_uuid, event->message.stream_id,
    EZGRPC2_GRPC_STATUS_INVALID_ARGUMENT, "Invalid name provided",
    &detail, 1,     /* details */
    nullptr, 0);    /* extra trailers */
```

### 5. Bearer token authentication

```c
static bool validate_token(const char *token, void *user_data) {
  return strcmp(token, "secret") == 0;
}

ezgrpc2_interceptor auth = ezgrpc2_interceptor_create_bearer_auth(validate_token, nullptr);
ezgrpc2_server_add_interceptor(server, &auth);

/* in the event loop, before running a handler: */
if (!ezgrpc2_server_intercept_pre(server, event, "SayHello"))
  continue; /* the stream was already ended with UNAUTHENTICATED */
```

---

## Complete Example & Integration Tests

[`examples/python_client_c_server/`](examples/python_client_c_server/) contains a full server and a Python test client:

- **`server.c`**: unary, server streaming, client streaming and bidirectional streaming handlers, trailers, rich errors, a logging interceptor, health checking and reflection. Gzip-compressed requests are decompressed by the library. Set `THREADS=N` to run N event loops on the same port.
- **`client.py`**: the official Python `grpcio` client, checking 9 scenarios over cleartext (`h2c`) and TLS.

[`examples/flatbuffers/`](examples/flatbuffers/) is a Greeter whose messages are FlatBuffers:

- **`server.c`**: unary and server-streaming methods built with flatcc, rejecting requests that fail verification.
- **`client.py`**: a `grpcio` client that encodes and decodes the messages with the official Python `flatbuffers` package, checking 6 scenarios. It needs `pip install flatbuffers`.

Run the tests:

```bash
# unit tests
make -C src/tests test

# Python integration tests, cleartext and TLS
make -C examples/python_client_c_server test
make -C examples/python_client_c_server test-tls

# FlatBuffers integration test (when built with FLATCC=1)
make -C examples/flatbuffers test

# everything
make test
```

---

## Continuous Integration (CI)

Every push and pull request is tested with GitHub Actions ([`.github/workflows/ci.yml`](.github/workflows/ci.yml)) on:

- **OS**: Ubuntu 24.04 and Ubuntu 22.04
- **Compilers**: GCC and Clang
- **Allocators**: libc `malloc`, `mimalloc` and `jemalloc`
- **Sanitizers**: AddressSanitizer builds (`ASAN=1`)
- **FlatBuffers**: flatcc built from source, with GCC, Clang and AddressSanitizer
- **TLS backends**: LibreSSL, BoringSSL, AWS-LC, wolfSSL, GnuTLS and rustls, in addition to OpenSSL
