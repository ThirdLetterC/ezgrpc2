#ifndef EZGRPC2_CORE_H
#define EZGRPC2_CORE_H

#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <string.h>
#include <assert.h>
#include <unistd.h>
#include <stdbool.h>
#include <pthread.h>
#include <uv.h>
#include <nghttp2/nghttp2.h>

#include "thpool.h"
#include "thpool_struct.h"

#include <uuid/uuid.h>
#include <arpa/inet.h>
#include <byteswap.h>
#include <fcntl.h>
#include <netdb.h>
#include <netinet/in.h>
#include <netinet/tcp.h>
#include <sys/socket.h>
#include <sys/types.h>

#include "ansicolors.h"
#include "common.h"
#include "ezgrpc2.h"
#include "ezgrpc2_http2_settings_struct.h"
#include "ezgrpc2_server_settings_struct.h"
#include "ezgrpc2_session_uuid_struct.h"
#include "ezgrpc2_list_struct.h"

/* stores the values of a SETTINGS frame */
// typedef struct ezgrpc_settingsf_t ezgrpc_settingsf_t;
// struct ezgrpc_settingsf_t {
//  u32 header_table_size;
//  u32 enable_push;
//  u32 max_concurrent_streams;
//  u32 flow_control;
//  u32 max_frame_size;
//};
//

/* a growable byte buffer */
typedef struct ezgrpc2_buf ezgrpc2_buf;
struct ezgrpc2_buf {
  u8 *data;
  size_t len;
  size_t cap;
};

/* small request messages are buffered inside the stream itself */
#define EZGRPC2_STREAM_INLINE_RECV 256

typedef struct ezgrpc2_stream ezgrpc2_stream;
struct ezgrpc2_stream {
  i32 stream_id;

  /* intrusive links of ezgrpc2_session.streams */
  ezgrpc2_stream *prev;
  ezgrpc2_stream *next;

  /* the time the stream is received by the server in
   * unix epoch */
  u64 time;
  size_t nb_headers;
  /* name and value point into the nghttp2 buffers held by `header_rcbufs`.
   * nghttp2 guarantees they are NULL-terminated. */
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
  uint64_t deadline_ms;

  /* point into `headers` */
  const char *hcontent_type;
  const char *hgrpc_encoding;
  const char *hgrpc_accept_encoding;
  const char *hgrpc_timeout;

  /* stores `:path` */
  ezgrpc2_path *path;

  /* recv_data. points to `recv_inline` until it outgrows it */
  size_t recv_len;
  size_t recv_cap;
  u8 *recv_data;
  u8 recv_inline[EZGRPC2_STREAM_INLINE_RECV];

  /* bytes of the front message of `lqueue_omessages` already
   * handed to nghttp2, counting its 5 byte prefix */
  size_t trunc_seek;
  ezgrpc2_list lqueue_omessages;

  /* trailers submitted by the user while DATA is still queued. they
   * are submitted once the data provider reaches EOF so that they
   * cannot overtake the DATA frames. */
  nghttp2_nv *pending_trailers;
  size_t nb_pending_trailers;
};

typedef struct ezgrpc2_session ezgrpc2_session;
struct ezgrpc2_session {
  nghttp2_session *ngsession;
  ezgrpc2_session_uuid session_uuid;
  int sockfd;
  socklen_t socklen;
  /* parent server context */
  ezgrpc2_server *server;

  struct sockaddr_storage sockaddr;

  /* an ASCII string */
  char client_addr[64];
  u16 client_port;

  char *server_addr;
  u16 *server_port;

  /* settings requested by the client. to be set when a SETTINGS
   * frame is received.
   */
  ezgrpc2_http2_settings client_http2_settings;

  /* the open streams. allocated when we are
   * about to receive a HEADERS frame */
  ezgrpc2_stream *streams;
  size_t nb_streams;

  struct ssl_st *ssl;
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

typedef struct ezgrpc2_interceptor ezgrpc2_interceptor;

struct ezgrpc2_server {
  ezgrpc2_server_settings server_settings;
  ezgrpc2_http2_settings http2_settings;
  thpool logger_thread;
  size_t nb_paths;
  ezgrpc2_path *paths;
  pthread_mutex_t mutex;

  nghttp2_session_callbacks *ngcallbacks;
  ezgrpc2_events *events;

  u16 ipv4_port;
  u16 ipv6_port;

  /* ASCII strings which represent where address to bind to */
  char *ipv4_addr;
  char *ipv6_addr;

  int ipv4_sockfd;
  int ipv6_sockfd;

  uv_loop_t loop;
  uv_tcp_t ipv4_listener;
  uv_tcp_t ipv6_listener;
  uv_timer_t timer;

  bool loop_initialized : 1;
  bool ipv4_listener_active : 1;
  bool ipv6_listener_active : 1;
  bool timer_active : 1;

  int ready_events;

  /* a loop is only ever driven by one thread, so every session reads
   * into this buffer and hands it to nghttp2 before the next read. */
  u8 *rbuf;
  size_t rbuf_len;
  /* TLS plaintext output, batched so that it is encrypted at once */
  ezgrpc2_buf tls_obuf;

  /* sessions with output to flush on the next poll */
  ezgrpc2_session *flush_head;

  /* the earliest stream deadline that may still be pending */
  u64 next_deadline_ms;

  size_t nb_sessions;
  ezgrpc2_session *sessions;
  /* stack of indices into `sessions` that are free to use */
  size_t nb_free_sessions;
  size_t *free_sessions;

  size_t nb_interceptors;
  ezgrpc2_interceptor *interceptors;

  struct ssl_ctx_st *ssl_ctx;
  bool is_tls : 1;
};

#endif
