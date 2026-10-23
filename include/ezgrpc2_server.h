#ifndef EZGRPC2_SERVER_H
#define EZGRPC2_SERVER_H

#include <stdio.h>
#include "ezgrpc2_call.h"
#include "ezgrpc2_defs.h"
#include "ezgrpc2_error.h"
#include "ezgrpc2_event.h"

typedef struct ezgrpc2_server ezgrpc2_server;

typedef enum ezgrpc2_log_level {
  /** EZGRPC2_LOG_INFO */
  EZGRPC2_LOG_DEFAULT = 0,
  EZGRPC2_LOG_OFF,
  EZGRPC2_LOG_ERROR,
  EZGRPC2_LOG_WARNING,
  /** startup and shutdown */
  EZGRPC2_LOG_INFO,
  /** connections */
  EZGRPC2_LOG_DEBUG,
  /** every HTTP/2 frame, when built with T=1 */
  EZGRPC2_LOG_TRACE,
} ezgrpc2_log_level;

/**
 * Server configuration. Zero fields take their default, so a designated
 * initializer only names what differs:
 *
 *   ezgrpc2_server *srv = ezgrpc2_server_new(&(ezgrpc2_config){.port = 50051});
 */
typedef struct ezgrpc2_config {
  /** Port for both address families. Required. */
  uint16_t port;
  /** IPv4 address to listen on. When both addresses are nullptr, the server
   * listens on "0.0.0.0" and "::". When only one is set, only that one. */
  const char *ipv4_addr;
  /** IPv6 address to listen on, see ipv4_addr. */
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
  /** Default EZGRPC2_LOG_INFO. */
  ezgrpc2_log_level log_level;
  /** Default stderr. */
  FILE *log_file;
} ezgrpc2_config;

/**
 * Handles a method whose client sends a single request: unary and
 * server-streaming methods. Called once the request is complete. Answer with
 * ezgrpc2_call_send() (any number of times) and ezgrpc2_call_finish(), now or
 * later.
 */
typedef void (*ezgrpc2_handler_fn)(ezgrpc2_call *call, const ezgrpc2_message *request,
                                   void *userdata);

/**
 * Handles a method whose client streams requests: client-streaming and
 * bidirectional methods. Messages are valid until the callback returns.
 */
typedef struct ezgrpc2_stream_handler {
  /** A request message arrived. */
  void (*on_message)(ezgrpc2_call *call, const ezgrpc2_message *message, void *userdata);
  /** The client finished sending. Optional. */
  void (*on_end)(ezgrpc2_call *call, void *userdata);
  /** The call was cancelled. Optional. */
  void (*on_cancel)(ezgrpc2_call *call, void *userdata);
} ezgrpc2_stream_handler;

/**
 * Creates a server and starts listening. ``config`` may be nullptr only to
 * fail: ``port`` is required. Returns nullptr on failure, after logging why.
 */
EZGRPC2_API ezgrpc2_server *ezgrpc2_server_new(const ezgrpc2_config *config);

/**
 * Closes every connection and frees the server. Release the references taken
 * on its calls first.
 */
EZGRPC2_API void ezgrpc2_server_free(ezgrpc2_server *server);

/**
 * Registers a handler for a single-request method, e.g.
 * ``"/helloworld.Greeter/SayHello"``.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_server_handle(ezgrpc2_server *server, const char *path,
                                              ezgrpc2_handler_fn handler, void *userdata);

/**
 * Registers handlers for a method whose client streams requests.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_server_handle_stream(ezgrpc2_server *server, const char *path,
                                                     const ezgrpc2_stream_handler *handler,
                                                     void *userdata);

/**
 * Registers a method whose calls are reported as events instead of
 * dispatched to a handler; see ezgrpc2_server_poll().
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_server_register_path(ezgrpc2_server *server, const char *path,
                                                     void *userdata);

/**
 * Stops accepting new calls to a method. Calls in progress are unaffected.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_server_unregister(ezgrpc2_server *server, const char *path);

/**
 * Serves until ezgrpc2_server_stop() is called.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_server_run(ezgrpc2_server *server);

/**
 * Makes ezgrpc2_server_run() return. Safe to call from any thread and from a
 * signal handler.
 */
EZGRPC2_API void ezgrpc2_server_stop(ezgrpc2_server *server);

/**
 * Runs one iteration of the event loop: writes the responses queued since
 * the last call, waits up to ``timeout_ms`` (-1: no limit, 0: no wait) for
 * network activity, and dispatches handlers.
 *
 * Returns the number of events of ezgrpc2_server_register_path() methods,
 * read with ezgrpc2_server_events(), or a negative ezgrpc2_err.
 */
EZGRPC2_API int ezgrpc2_server_poll(ezgrpc2_server *server, int timeout_ms);

/**
 * The events of the last ezgrpc2_server_poll(), valid until the next one.
 */
EZGRPC2_API const ezgrpc2_event *ezgrpc2_server_events(ezgrpc2_server *server, size_t *count);

/**
 * Writes the queued responses now instead of at the next poll.
 */
EZGRPC2_API void ezgrpc2_server_flush(ezgrpc2_server *server);

/**
 * Runs ``fn(arg)`` on the server thread soon. Safe to call from any thread:
 * this is how worker threads hand results back.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_server_post(ezgrpc2_server *server, void (*fn)(void *arg),
                                            void *arg);

/**
 * The TLS library's own context, see ezgrpc2_tls.h. nullptr without TLS.
 */
EZGRPC2_API void *ezgrpc2_server_tls_native(ezgrpc2_server *server);

/**
 * The license text of ezgrpc2.
 */
EZGRPC2_API const char *ezgrpc2_license(void);

#endif /* EZGRPC2_SERVER_H */
