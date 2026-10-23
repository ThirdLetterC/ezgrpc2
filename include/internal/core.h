#ifndef EZGRPC2_CORE_H
#define EZGRPC2_CORE_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <pthread.h>
#include <stdatomic.h>
#include <stdbool.h>
#include <string.h>
#include <unistd.h>
#include <uv.h>
#include <nghttp2/nghttp2.h>

#include <arpa/inet.h>
#include <fcntl.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "ezgrpc2.h"
#include "ansicolors.h"
#include "common.h"
#include "ezgrpc2_header.h"
#include "ezgrpc2_list.h"
#include "ezgrpc2_list_struct.h"
#include "ezgrpc2_messages.h"

/* ---- logging ------------------------------------------------------------- */

void server_log(ezgrpc2_server *server, ezgrpc2_log_level level, const char *fmt, ...)
    __attribute__((format(printf, 3, 4)));

#define LOG_ERROR(server, fmt, ...)                                                                \
  server_log(server, EZGRPC2_LOG_ERROR, COLSTR("error: " fmt, BHRED) __VA_OPT__(, ) __VA_ARGS__)
#define LOG_WARNING(server, fmt, ...)                                                              \
  server_log(server, EZGRPC2_LOG_WARNING,                                                          \
             COLSTR("warning: " fmt, BHYEL) __VA_OPT__(, ) __VA_ARGS__)
#define LOG_INFO(server, ...)  server_log(server, EZGRPC2_LOG_INFO, __VA_ARGS__)
#define LOG_DEBUG(server, ...) server_log(server, EZGRPC2_LOG_DEBUG, __VA_ARGS__)
/* per frame: only compiled in with T=1 */
#ifdef EZGRPC2_TRACE
#define LOG_TRACE(server, fmt, ...)                                                                \
  server_log(server, EZGRPC2_LOG_TRACE, "@%s: " fmt, __func__ __VA_OPT__(, ) __VA_ARGS__)
#else
#define LOG_TRACE(server, ...)                                                                     \
  do {                                                                                             \
  } while (0)
#endif

/* ---- buffers --------------------------------------------------------------- */

/* a growable byte buffer */
typedef struct ezgrpc2_buf ezgrpc2_buf;
struct ezgrpc2_buf {
  u8 *data;
  size_t len;
  size_t cap;
};

/* ---- routes ---------------------------------------------------------------- */

typedef enum route_kind {
  ROUTE_UNARY,  /* ezgrpc2_server_handle() */
  ROUTE_STREAM, /* ezgrpc2_server_handle_stream() */
  ROUTE_EVENTS, /* ezgrpc2_server_register_path() */
} route_kind;

/* a registered method. never moved or freed while the server lives, since
 * calls point to it */
typedef struct ezgrpc2_route ezgrpc2_route;
struct ezgrpc2_route {
  char *path;
  size_t path_len;
  route_kind kind;
  ezgrpc2_handler_fn handler;
  ezgrpc2_stream_handler stream;
  void *userdata;
  /* ezgrpc2_server_unregister(): new calls get UNIMPLEMENTED */
  bool disabled;
};

/* ---- calls ----------------------------------------------------------------- */

typedef struct ezgrpc2_stream ezgrpc2_stream;
typedef struct ezgrpc2_session ezgrpc2_session;

/* an owned list of headers to send */
typedef struct nv_list nv_list;
struct nv_list {
  nghttp2_nv *nva;
  size_t len;
  size_t cap;
};

struct ezgrpc2_call {
  atomic_size_t refs;
  ezgrpc2_server *server;
  ezgrpc2_route *route;

  /* the HTTP/2 stream, until it closes */
  ezgrpc2_session *session;
  ezgrpc2_stream *stream;
  i32 stream_id;

  u64 start_ms;
  /* the client's deadline, 0 if none */
  u64 deadline_ms;
  char peer[INET6_ADDRSTRLEN + 8];

  /* a copy of the request headers, valid as long as the call */
  ezgrpc2_header *req_headers;
  size_t nb_req_headers;

  /* received messages not yet given to the application */
  ezgrpc2_list pending;
  /* the request of a ROUTE_UNARY call */
  ezgrpc2_message *request;

  /* response headers and trailers set by the application */
  nv_list headers;
  nv_list trailers;

  ezgrpc2_status cancel_reason;

  bool headers_sent : 1;
  /* grpc-encoding: gzip was announced */
  bool gzip : 1;
  bool finished : 1;
  bool closed : 1;
  bool cancelled : 1;
  bool cancel_delivered : 1;
  /* the client half-closed */
  bool half_closed : 1;
  bool end_delivered : 1;
  /* interceptors ran and the application has seen the call */
  bool started : 1;
  bool rejected : 1;
  bool request_delivered : 1;
  /* interceptors' on_done ran */
  bool done_reported : 1;
  bool queued : 1;

  void *userdata;
  void (*userdata_free)(void *);
  void (*cancel_fn)(ezgrpc2_call *, void *);
  void *cancel_userdata;

  /* ezgrpc2_server.ready_head */
  ezgrpc2_call *ready_next;
};

/* ---- streams and sessions ----------------------------------------------------- */

/* small request messages are buffered inside the stream itself */
#define EZGRPC2_STREAM_INLINE_RECV 256

struct ezgrpc2_stream {
  i32 stream_id;

  /* intrusive links of ezgrpc2_session.streams */
  ezgrpc2_stream *prev;
  ezgrpc2_stream *next;

  size_t nb_headers;
  /* name and value point into the nghttp2 buffers held by `header_rcbufs` */
  ezgrpc2_header headers[CONFIG_STREAM_MAX_HEADERS];
  nghttp2_rcbuf *header_rcbufs[CONFIG_STREAM_MAX_HEADERS][2];

  bool is_method_post : 1;
  bool is_scheme_http : 1;
  /* just a bool. if `te` is a trailer, or `te` does exists. */
  bool is_te_trailers : 1;
  /* just a bool. if content type is application/grpc* */
  bool is_content_grpc : 1;
  bool is_deadline_set : 1;
  /* a data provider is submitted to nghttp2 and has not returned EOF yet */
  bool is_data_active : 1;
  /* the request was rejected; ignore what the client still sends */
  bool is_discarding : 1;
  uint64_t deadline_ms;

  /* point into `headers` */
  const char *hcontent_type;
  const char *hgrpc_encoding;
  const char *hgrpc_accept_encoding;
  const char *hgrpc_timeout;

  /* the method, set from `:path` */
  ezgrpc2_route *route;
  /* the call of a routed, valid request */
  ezgrpc2_call *call;

  /* recv_data. points to `recv_inline` until it outgrows it */
  size_t recv_len;
  size_t recv_cap;
  u8 *recv_data;
  u8 recv_inline[EZGRPC2_STREAM_INLINE_RECV];

  /* bytes of the front message of `lqueue_omessages` already
   * handed to nghttp2, counting its 5 byte prefix */
  size_t trunc_seek;
  ezgrpc2_list lqueue_omessages;

  /* trailers submitted while DATA is still queued. they are submitted once
   * the data provider reaches EOF so that they cannot overtake the DATA */
  nghttp2_nv *pending_trailers;
  size_t nb_pending_trailers;
};

struct ezgrpc2_session {
  nghttp2_session *ngsession;
  /* the index in ezgrpc2_server.sessions */
  size_t index;
  int sockfd;
  socklen_t socklen;
  /* parent server context */
  ezgrpc2_server *server;

  struct sockaddr_storage sockaddr;
  /* "ip:port" or "[ip]:port" */
  char peer[INET6_ADDRSTRLEN + 8];

  /* the open streams. allocated when we are
   * about to receive a HEADERS frame */
  ezgrpc2_stream *streams;
  size_t nb_streams;

  struct ezgrpc2_tls_session *tls;
  bool is_tls : 1;
  bool tls_handshake_done : 1;

  uv_tcp_t tcp;
  bool is_active : 1;
  bool is_closing : 1;

  /* bytes serialized by nghttp2 (and encrypted, if TLS) that are not
   * yet accepted by the kernel */
  ezgrpc2_buf obuf;
  /* the buffer owned by the in-flight `write_req` */
  ezgrpc2_buf wbuf;
  uv_write_t write_req;
  bool is_writing : 1;

  /* links of ezgrpc2_server.flush_head. these survive a session
   * reset since a closed slot may still be queued. */
  bool is_flush_queued : 1;
  ezgrpc2_session *flush_next;
};

/* ---- the server ------------------------------------------------------------ */

typedef struct post_task post_task;
struct post_task {
  void (*fn)(void *);
  void *arg;
  post_task *next;
};

struct health_state;
struct reflection_state;

struct ezgrpc2_server {
  /* the configuration, with defaults applied */
  ezgrpc2_config config;
  char *ipv4_addr;
  char *ipv6_addr;

  ezgrpc2_log_level log_level;
  FILE *log_file;

  ezgrpc2_route **routes;
  size_t nb_routes;
  size_t cap_routes;

  nghttp2_session_callbacks *ngcallbacks;

  int ipv4_sockfd;
  int ipv6_sockfd;

  uv_loop_t loop;
  uv_tcp_t ipv4_listener;
  uv_tcp_t ipv6_listener;
  /* the timeout of ezgrpc2_server_poll() */
  uv_timer_t timer;
  /* fires at the earliest stream deadline */
  uv_timer_t deadline_timer;
  /* ezgrpc2_server_post() and ezgrpc2_server_stop() */
  uv_async_t async;

  bool loop_initialized : 1;
  bool ipv4_listener_active : 1;
  bool ipv6_listener_active : 1;
  bool timer_active : 1;
  bool deadline_timer_active : 1;
  bool async_active : 1;
  bool is_tls : 1;

  atomic_bool stop;

  int ready_events;

  /* a loop is only ever driven by one thread, so every session reads
   * into this buffer and hands it to nghttp2 before the next read. */
  u8 *rbuf;
  size_t rbuf_len;
  /* TLS plaintext output, batched so that it is encrypted at once */
  ezgrpc2_buf tls_obuf;

  /* sessions with output to flush on the next poll */
  ezgrpc2_session *flush_head;

  /* the deadline the timer is armed for, UINT64_MAX if none */
  u64 next_deadline_ms;

  size_t nb_sessions;
  ezgrpc2_session *sessions;
  /* stack of indices into `sessions` that are free to use */
  size_t nb_free_sessions;
  size_t *free_sessions;

  size_t nb_interceptors;
  ezgrpc2_interceptor *interceptors;

  /* calls with something to give to the application */
  ezgrpc2_call *ready_head;
  ezgrpc2_call *ready_tail;

  /* the events of the last poll. they hold a reference to their call and
   * own their message */
  ezgrpc2_event *events;
  size_t nb_events;
  size_t cap_events;

  /* ezgrpc2_server_post() */
  pthread_mutex_t post_mutex;
  post_task *post_head;
  post_task *post_tail;

  struct health_state *health;
  struct reflection_state *reflection;

  struct ezgrpc2_tls_ctx *tls_ctx;
};

/* ---- internal functions ------------------------------------------------------ */

/* ezgrpc2_call.c */
ezgrpc2_call *call_new(ezgrpc2_session *session, ezgrpc2_stream *stream);
/* the stream is gone; cancels the call unless it was finished */
void call_detach(ezgrpc2_call *call, ezgrpc2_status reason);
/* ends the call with `status` on behalf of the library, and cancels it */
void call_abort(ezgrpc2_call *call, ezgrpc2_status status, const char *message);
/* queues the call for server_dispatch() */
void call_queue(ezgrpc2_call *call);
/* gives the queued calls to their handlers, or turns them into events */
void server_dispatch(ezgrpc2_server *server);
/* releases the events of the previous poll */
void server_clear_events(ezgrpc2_server *server);
/* drops the queued calls without dispatching them */
void server_drop_ready(ezgrpc2_server *server);

/* ezgrpc2_server.c */
ezgrpc2_route *server_find_route(ezgrpc2_server *server, const char *path, size_t len);
ezgrpc2_err server_add_route(ezgrpc2_server *server, const char *path, route_kind kind,
                             ezgrpc2_handler_fn handler, const ezgrpc2_stream_handler *stream,
                             void *userdata);

/* ezgrpc2_interceptor.c */
/* runs the on_call interceptors; false if one ended the call */
bool interceptors_run_on_call(ezgrpc2_call *call);
void interceptors_run_on_done(ezgrpc2_call *call, ezgrpc2_status status);
void interceptors_free(ezgrpc2_server *server);

/* ezgrpc2_health.c, ezgrpc2_reflection.c */
void health_free(ezgrpc2_server *server);
void reflection_free(ezgrpc2_server *server);

/* ezgrpc2_tls.c */
ezgrpc2_err tls_enable(ezgrpc2_server *server, const char *cert_file, const char *key_file);
void tls_free(ezgrpc2_server *server);

/* ezgrpc2_message.c: a message with a compressed flag */
ezgrpc2_message *message_create(u8 cflag, const void *data, size_t len);

#endif
