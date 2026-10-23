#pragma once

#include <stdio.h>
#include "grpc_call.h"
#include "grpc_defs.h"
#include "grpc_error.h"
#include "grpc_event.h"

typedef struct grpc_server grpc_server;

typedef enum grpc_log_level {
  /** Use the default level, GRPC_LOG_INFO. */
  GRPC_LOG_DEFAULT = 0,
  GRPC_LOG_OFF,
  GRPC_LOG_ERROR,
  GRPC_LOG_WARNING,
  /** Startup and shutdown messages. */
  GRPC_LOG_INFO,
  /** Connection messages. */
  GRPC_LOG_DEBUG,
  /** Every HTTP/2 frame, when built with T=1. */
  GRPC_LOG_TRACE,
} grpc_log_level;

/**
 * Server configuration. Zero fields take their default, so a designated
 * initializer only names what differs:
 *
 *   grpc_server *srv = grpc_server_new(&(grpc_config){.port = 50051});
 */
typedef struct grpc_config {
  /** Port for both address families. Required. */
  uint16_t port;
  /** IPv4 address to listen on. When both addresses are nullptr, the server
   * listens on "0.0.0.0" and "::". When only one is set, only that one. */
  const char *ipv4_addr;
  /** IPv6 address to listen on; see ipv4_addr. */
  const char *ipv6_addr;
  /** listen() backlog. Default 1024. */
  int backlog;
  /** Concurrent connections. Default 1024. */
  size_t max_connections;
  /** Concurrent calls per connection. Default 1024. */
  uint32_t max_concurrent_streams;
  /** Largest request message, in bytes. Default 1 MiB. */
  uint32_t max_message_size;
  /** Largest HTTP/2 frame, in bytes. Default 16 KiB. */
  uint32_t max_frame_size;
  /** PEM certificate chain and private key. Setting both enables TLS. */
  struct {
    const char *cert_file;
    const char *key_file;
  } tls;
  /** Initial log threshold. Defaults to GRPC_LOG_INFO. */
  grpc_log_level log_level;
  /** Log output stream. Defaults to stderr. */
  FILE *log_file;
} grpc_config;

/**
 * Handles a method whose client sends a single request: unary and
 * server-streaming methods. Called once the request is complete. Answer with
 * grpc_call_send() (any number of times) and grpc_call_finish(), now or
 * later.
 */
typedef void (*grpc_handler_fn)(grpc_call *call, const grpc_message *request,
                                   void *userdata);

/**
 * Handles a method whose client streams requests: client-streaming and
 * bidirectional methods. Messages are valid until the callback returns.
 */
typedef struct grpc_stream_handler {
  /** A request message arrived. */
  void (*on_message)(grpc_call *call, const grpc_message *message, void *userdata);
  /** The client finished sending. Optional. */
  void (*on_end)(grpc_call *call, void *userdata);
  /** The call was cancelled. Optional. */
  void (*on_cancel)(grpc_call *call, void *userdata);
} grpc_stream_handler;

/**
 * Creates a server and starts listening. ``config`` may be nullptr only to
 * fail: ``port`` is required. Returns nullptr on failure, after logging why.
 */
GRPC_API grpc_server *grpc_server_new(const grpc_config *config);

/**
 * Closes every connection and frees the server. Release the references taken
 * on its calls first.
 */
GRPC_API void grpc_server_free(grpc_server *server);

/**
 * Changes the server's log threshold. Safe to call from any thread while the
 * server is alive. GRPC_LOG_DEFAULT selects GRPC_LOG_INFO.
 */
GRPC_API grpc_err grpc_server_set_log_level(grpc_server *server,
                                                     grpc_log_level level);

/**
 * Registers a handler for a single-request method, e.g.
 * ``"/helloworld.Greeter/SayHello"``.
 */
GRPC_API grpc_err grpc_server_handle(grpc_server *server, const char *path,
                                              grpc_handler_fn handler, void *userdata);

/**
 * Registers handlers for a method whose client streams requests.
 */
GRPC_API grpc_err grpc_server_handle_stream(grpc_server *server, const char *path,
                                                     const grpc_stream_handler *handler,
                                                     void *userdata);

/**
 * Registers a method whose calls are reported as events instead of
 * dispatched to a handler; see grpc_server_poll().
 */
GRPC_API grpc_err grpc_server_register_path(grpc_server *server, const char *path,
                                                     void *userdata);

/**
 * Stops accepting new calls to a method. Calls in progress are unaffected.
 */
GRPC_API grpc_err grpc_server_unregister(grpc_server *server, const char *path);

/**
 * Serves until grpc_server_stop() is called.
 */
GRPC_API grpc_err grpc_server_run(grpc_server *server);

/**
 * Makes grpc_server_run() return. Safe to call from any thread and from a
 * signal handler.
 */
GRPC_API void grpc_server_stop(grpc_server *server);

/**
 * Runs one iteration of the event loop: writes the responses queued since
 * the last call, waits up to ``timeout_ms`` (-1: no limit, 0: no wait) for
 * network activity, and dispatches handlers.
 *
 * Returns the number of events of grpc_server_register_path() methods,
 * read with grpc_server_events(), or a negative grpc_err.
 */
GRPC_API int grpc_server_poll(grpc_server *server, int timeout_ms);

/**
 * The events of the last grpc_server_poll(), valid until the next one.
 */
GRPC_API const grpc_event *grpc_server_events(grpc_server *server, size_t *count);

/**
 * Writes the queued responses now instead of at the next poll.
 */
GRPC_API void grpc_server_flush(grpc_server *server);

/**
 * Runs ``fn(arg)`` on the server thread soon. Safe to call from any thread:
 * this is how worker threads hand results back.
 */
GRPC_API grpc_err grpc_server_post(grpc_server *server, void (*fn)(void *arg),
                                            void *arg);

/**
 * The TLS library's own context, see grpc_tls.h. nullptr without TLS.
 */
GRPC_API void *grpc_server_tls_native(grpc_server *server);

/**
 * The license text of grpc.
 */
GRPC_API const char *grpc_license(void);
