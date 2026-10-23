#ifndef _GNU_SOURCE
#define _GNU_SOURCE
#endif

#include <assert.h>
#include <errno.h>
#include <time.h>
#include <stddef.h>
#include <strings.h>
#include <stdio.h>
#include "core.h"
#include "ezgrpc2_header.h"
#include "internal_nghttp2_callbacks.h"
#include "internal_helpers.h"
#include "ezgrpc2_events_struct.h"
#include "ezgrpc2_tls.h"

#ifdef _WIN32
int makenonblock(SOCKET sockfd) {
  return ioctlsocket(sockfd, FIONBIO, &(u_long){1});
}
#else
int makenonblock(int sockfd) {
  int res = fcntl(sockfd, F_GETFL, 0);
  if (res == -1) return 1;
  res = (res | O_NONBLOCK);
  return fcntl(sockfd, F_SETFL, res);
}
#endif

int list_cmp_ezheader_name(const void *data, const void *userdata) {
  const ezgrpc2_header *a = data;
  const ezgrpc2_header *b = userdata;
  return a->namelen == b->namelen ?
      strncasecmp(a->name, b->name, a->namelen) : 1;
}

static void dump_decode_binary(const i8 *data, size_t len) {
  const i8 look_up[16] = {'0', '1', '2', '3', '4', '5', '6', '7',
                          '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'};
  for (size_t i = 0; i < len; i++) {
    putchar(look_up[(data[i] >> 4) & 0x0f]);
    putchar(look_up[data[i] & 0x0f]);
    putchar(' ');
    if (i == 7) putchar(' ');
  }
}

static void dump_decode_ascii(const i8 *data, size_t len) {
  for (size_t i = 0; i < len; i++) {
    if (data[i] < 0x20) {
      putchar('.');
    } else if (data[i] < 0x7f)
      putchar(data[i]);
    else
      putchar('.');
  }
}

void ezdump(void *vdata, size_t len) {
  const i8 *data = vdata;
  if (len == 0) return;

  size_t cur = 0;

  for (; cur < 16 * (len / 16); cur += 16) {
    dump_decode_binary(data + cur, 16);
    printf("| ");
    dump_decode_ascii(data + cur, 16);
    printf(" |");
    puts("");
  }
  /* write the remaining */
  if (len % 16) {
    dump_decode_binary(data + cur, len % 16);
    /* write the empty */
    for (size_t i = 0; i < 16 - (len % 16); i++) {
      printf("   ");
      if (i == 7 && (7 < 16 - (len % 16))) putchar(' ');
    }
    printf("| ");
    dump_decode_ascii(data + cur, len % 16);
    for (size_t i = 0; i < 16 - (len % 16); i++) {
      putchar(' ');
    }
    printf(" |");
    puts("");
    cur += len % 16;
  }
  assert(len == cur);
}

char *ezgetdt(char *buf, size_t len) {
#ifdef __unix__
  struct timespec ts;
  clock_gettime(CLOCK_REALTIME, &ts);
  char tmp[32] = {};
  strftime(tmp, sizeof(tmp), "%F %T", localtime(&ts.tv_sec));
  snprintf(buf, len, COLSTR("%s.%09ld", BGRN), tmp, ts.tv_nsec);
#else
  time_t t = time(nullptr);
  struct tm stm = *localtime(&t);
  snprintf(buf, len, COLSTR("%d/%02d/%02d %02d:%02d:%02d", BGRN),
           stm.tm_year + 1900, stm.tm_mon + 1, stm.tm_mday, stm.tm_hour,
           stm.tm_min, stm.tm_sec);
#endif
  return buf;
}

void ezlog(const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char dt[128] = {};
  fprintf(stderr, "[%s] ", ezgetdt(dt, sizeof(dt)));
  vfprintf(stderr, fmt, args);
  va_end(args);
}

void ezflog(FILE *fp, const char *fmt, ...) {
  va_list args;
  va_start(args, fmt);
  char dt[128] = {};
  fprintf(fp, "[%s] ", ezgetdt(dt, sizeof(dt)));
  vfprintf(fp, fmt, args);
  va_end(args);
}

size_t get_unused_session_ndx(ezgrpc2_server *server) {
  for (size_t i = 0; i < server->nb_sessions; i++)
    if (!server->sessions[i].is_active && !server->sessions[i].is_closing)
      return i;
  return (size_t)-1;
}

void session_free(ezgrpc2_session *ezsession) {
  if (ezsession == nullptr)
    return;

  if (ezsession->is_active && ezsession->server && ezsession->server->events) {
    ezgrpc2_event event = {};
    event.session_uuid = ezsession->session_uuid;
    event.type = EZGRPC2_EVENT_DISCONNECT;
    ezgrpc2_events_write(ezsession->server->events, event);
  }

  if (ezsession->ngsession != nullptr) {
    nghttp2_session_terminate_session(ezsession->ngsession, 0);
    nghttp2_session_del(ezsession->ngsession);
    ezsession->ngsession = nullptr;
  }

  if (ezsession->lstreams != nullptr) {
    ezgrpc2_stream *ezstream;
    while ((ezstream = ezgrpc2_list_pop_front(ezsession->lstreams)) != nullptr)
      stream_free(ezstream);
    ezgrpc2_list_free(ezsession->lstreams);
    ezsession->lstreams = nullptr;
  }

  if (ezsession->is_tls) {
    ezgrpc2_tls_session_free(ezsession);
    ezsession->is_tls = false;
  }

  ezsession->sockfd = EZINVALID_SOCKET;
  ezsession->nb_streams = 0;
  ezsession->tls_handshake_done = false;
  memset(&ezsession->session_uuid, 0, sizeof(ezsession->session_uuid));
  memset(&ezsession->sockaddr, 0, sizeof(ezsession->sockaddr));
  memset(ezsession->client_addr, 0, sizeof(ezsession->client_addr));
  ezsession->client_port = 0;
}

static void on_session_poll_close(uv_handle_t *handle) {
  ezgrpc2_session *session = (ezgrpc2_session *)handle->data;
  if (session != nullptr) {
    session->is_closing = false;
    session->is_active = false;
  }
}

void session_close_and_free(ezgrpc2_session *session) {
  if (session == nullptr || !session->is_active || session->is_closing) {
    return;
  }
  session->is_closing = true;

  uv_poll_stop(&session->poll_handle);
  if (session->sockfd != EZINVALID_SOCKET) {
    shutdown(session->sockfd, SHUT_RDWR);
    close(session->sockfd);
    session->sockfd = EZINVALID_SOCKET;
  }
  session_free(session);
  uv_close((uv_handle_t *)&session->poll_handle, on_session_poll_close);
}

ezgrpc2_session *session_find(ezgrpc2_session *sessions, size_t nb_sessions, ezgrpc2_session_uuid session_uuid) {
  if (session_uuid.index >= nb_sessions)
    return nullptr;
  ezgrpc2_session *s = &sessions[session_uuid.index];
  if (!s->is_active || s->is_closing)
    return nullptr;
  return ezgrpc2_session_uuid_is_equal(session_uuid, s->session_uuid) ? s : nullptr;
}

static void *nghttp2_mem_malloc(size_t size, [[maybe_unused]] void *user_data) {
  return ezgrpc2_malloc(size);
}

static void nghttp2_mem_free(void *ptr, [[maybe_unused]] void *user_data) {
  ezgrpc2_free(ptr);
}

static void *nghttp2_mem_calloc(size_t nmemb, size_t size, [[maybe_unused]] void *user_data) {
  return ezgrpc2_calloc(nmemb, size);
}

static void *nghttp2_mem_realloc(void *ptr, size_t size, [[maybe_unused]] void *user_data) {
  return ezgrpc2_realloc(ptr, size);
}

int session_create(
    ezgrpc2_session *ezsession,
    EZSOCKET sockfd,
    struct sockaddr_storage *sockaddr,
    EZSOCKLEN socklen,
    ezgrpc2_server *server) {

  /* PREPARE NGHTTP2 */
  nghttp2_session_callbacks *ngcallbacks;
  int res = nghttp2_session_callbacks_new(&ngcallbacks);
  if (res) {
    return 1;
  }
  ezsession->server = server;

  /**********************.
  |     INIT NGHTTP2     |
  `---------------------*/
  server_setup_ngcallbacks(ngcallbacks);

  nghttp2_mem mem = {
      .mem_user_data = nullptr,
      .malloc = nghttp2_mem_malloc,
      .free = nghttp2_mem_free,
      .calloc = nghttp2_mem_calloc,
      .realloc = nghttp2_mem_realloc,
  };
  res = nghttp2_session_server_new3(&ezsession->ngsession, ngcallbacks, ezsession, nullptr, &mem);
  nghttp2_session_callbacks_del(ngcallbacks);
  if (res < 0) {
    return 1;
  }

  /* settings */
  nghttp2_settings_entry siv[] = {
      {NGHTTP2_SETTINGS_INITIAL_WINDOW_SIZE, (uint32_t)ezsession->server->http2_settings.initial_window_size},
      {NGHTTP2_SETTINGS_MAX_FRAME_SIZE, (uint32_t)ezsession->server->http2_settings.max_frame_size},
      {NGHTTP2_SETTINGS_MAX_CONCURRENT_STREAMS, (uint32_t)ezsession->server->http2_settings.max_concurrent_streams},
  };
  res = nghttp2_submit_settings(ezsession->ngsession, NGHTTP2_FLAG_NONE, siv, 3);
  if (res < 0) {
    assert(0);
  }

  if (makenonblock(sockfd)) assert(0);

#ifdef _WIN32
  res = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (char *)&(DWORD){1}, sizeof(DWORD));
  if (res < 0) {
    assert(0);
  }

  res = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (char *)&(BOOL){1}, sizeof(BOOL));
  if (res < 0) {
    assert(0);
  }
#else
  res = setsockopt(sockfd, IPPROTO_TCP, TCP_NODELAY, (void *)&(int){1}, sizeof(int));
  if (res < 0) {
    assert(0);
  }

  res = setsockopt(sockfd, SOL_SOCKET, SO_KEEPALIVE, (void *)&(int){1}, sizeof(int));
  if (res < 0) {
    assert(0);
  }
#endif

  ezsession->sockfd = sockfd;
  ezsession->sockaddr = *sockaddr;
  ezsession->socklen = socklen;
  switch (sockaddr->ss_family) {
    case AF_INET: {
      struct sockaddr_in *sa = (struct sockaddr_in *)sockaddr;
      inet_ntop(AF_INET, &sa->sin_addr.s_addr,
                ezsession->client_addr, sizeof(ezsession->client_addr) - 1);
      ezsession->client_port = ntohs(sa->sin_port);
    } break;
    case AF_INET6: {
      struct sockaddr_in6 *sa6 = (struct sockaddr_in6 *)sockaddr;
      inet_ntop(AF_INET6, sa6->sin6_addr.s6_addr,
                ezsession->client_addr, sizeof(ezsession->client_addr) - 1);
      ezsession->client_port = ntohs(sa6->sin6_port);
    } break;
    default:
      abort();
  }
  /* all seems to be successful */
  ezsession->lstreams = ezgrpc2_list_new(nullptr);
#ifdef _WIN32
  RPC_STATUS wres = UuidCreate(&ezsession->session_uuid.uuid);
  assert(wres == RPC_S_OK);
#else
  uuid_generate_random(ezsession->session_uuid.uuid);
#endif

  if (server->is_tls) {
    if (ezgrpc2_tls_session_init(ezsession) != 0) {
      return 1;
    }
  }

  return res;
}

void on_session_poll(uv_poll_t *handle, int status, int events) {
  ezgrpc2_session *session = (ezgrpc2_session *)handle->data;
  if (session == nullptr || !session->is_active || session->is_closing) {
    return;
  }
  ezgrpc2_server *server = session->server;
  server->ready_events++;

  if (events & UV_READABLE) {
    if (session_events(session)) {
      EZGRPC2_LOG_TRACE(server, "closing session %p", (void *)session);
      session_close_and_free(session);
      return;
    }
  }

  if (status < 0 || (events & UV_DISCONNECT)) {
    session_close_and_free(session);
    return;
  }
}

int session_add(ezgrpc2_server *ezserver, EZSOCKET listenfd) {
  struct sockaddr_storage sockaddr;
  EZSOCKLEN sockaddr_len = sizeof(sockaddr);
  memset(&sockaddr, 0, sizeof(sockaddr));
  EZSOCKET confd;
  while ((confd = accept(listenfd, (struct sockaddr *)&sockaddr, &sockaddr_len)) == EZINVALID_SOCKET && errno == EINTR);
  if (confd == EZINVALID_SOCKET) {
    if (errno != EAGAIN && errno != EWOULDBLOCK) {
      EZGRPC2_LOG_ERROR(ezserver, "accept: %s", strerror(errno));
    }
    return 1;
  }

  EZGRPC2_LOG_NORMAL(ezserver, "incoming %s connection\n", sockaddr.ss_family == AF_INET ? "ipv4" : "ipv6");

  size_t ndx = get_unused_session_ndx(ezserver);
  if (ndx == (size_t)-1) {
    EZGRPC2_LOG_ERROR(ezserver, "@ %s: max client reached", __func__);
    shutdown(confd, SHUT_RDWR);
    close(confd);
    return 1;
  }

  ezgrpc2_session *session = &ezserver->sessions[ndx];
  if (session_create(session, confd, &sockaddr, sockaddr_len, ezserver)) {
    EZGRPC2_LOG_ERROR(ezserver, "@ %s: session_create failed", __func__);
    session_free(session);
    shutdown(confd, SHUT_RDWR);
    close(confd);
    return 1;
  }
  session->session_uuid.index = ndx;
  session->is_active = true;
  session->is_closing = false;

  int uv_res = uv_poll_init_socket(&ezserver->loop, &session->poll_handle, confd);
  if (uv_res != 0) {
    session_free(session);
    close(confd);
    session->is_active = false;
    return 1;
  }
  session->poll_handle.data = session;

  uv_res = uv_poll_start(&session->poll_handle, UV_READABLE | UV_DISCONNECT, on_session_poll);
  if (uv_res != 0) {
    session_close_and_free(session);
    return 1;
  }

  if (ezserver->events != nullptr) {
    ezgrpc2_event event = {};
    event.type = EZGRPC2_EVENT_CONNECT;
    event.session_uuid = session->session_uuid;
    ezgrpc2_events_write(ezserver->events, event);
  }

  return 0;
}

void on_listen_poll(uv_poll_t *handle, int status, int events) {
  (void)events;
  if (status < 0) {
    return;
  }
  ezgrpc2_server *server = (ezgrpc2_server *)handle->data;
  uv_os_sock_t fd;
  if (uv_fileno((uv_handle_t *)handle, &fd) != 0) {
    return;
  }
  while (session_add(server, (EZSOCKET)fd) == 0) {
    server->ready_events++;
  }
}

int session_events(ezgrpc2_session *ezsession) {
  int res;

  if (ezsession->is_tls && !ezsession->tls_handshake_done) {
    int hres = ezgrpc2_tls_handshake(ezsession);
    if (hres == 0) {
      return 0; // Handshake in progress
    } else if (hres < 0) {
      EZGRPC2_LOG_ERROR(ezsession->server, "TLS handshake failed on session %p\n", (void *)ezsession);
      return 1;
    }
  }

  if (!nghttp2_session_want_read(ezsession->ngsession) &&
      !nghttp2_session_want_write(ezsession->ngsession))
    return 1;

  if (nghttp2_session_want_read(ezsession->ngsession) &&
      (res = nghttp2_session_recv(ezsession->ngsession))) {
    ezgrpc2_server_log(ezsession->server, EZGRPC2_SERVER_LOG_ERROR, COLSTR("@ %s: nghttp2: %s. killing...\n", BHRED), __func__,
          nghttp2_strerror(res));
    return 1;
  }

  while (nghttp2_session_want_write(ezsession->ngsession)) {
    res = nghttp2_session_send(ezsession->ngsession);
    if (res) {
      EZGRPC2_LOG_ERROR(ezsession->server, "nghttp2: %s. killing...\n",
            nghttp2_strerror(res));
      return 1;
    }
  }
  return 0;
}

ezgrpc2_stream *stream_new(i32 stream_id) {
  ezgrpc2_stream *ezstream = calloc(1, sizeof(*ezstream));
  ezstream->lqueue_omessages = ezgrpc2_list_new(nullptr);
  ezstream->nb_headers = 0;
  ezstream->time = (uint64_t)time(nullptr);
  ezstream->is_trunc = 0;
  ezstream->trunc_seek = 0;
  ezstream->stream_id = stream_id;
  return ezstream;
}

void stream_free(ezgrpc2_stream *ezstream) {
  if (ezstream == nullptr) return;
  free(ezstream->recv_data);
  free(ezstream->hgrpc_encoding);
  free(ezstream->hgrpc_accept_encoding);
  free(ezstream->hcontent_type);
  free(ezstream->hgrpc_timeout);

  for (size_t i = 0; i < ezstream->nb_headers; i++) {
    free(ezstream->headers[i].value);
    free(ezstream->headers[i].name);
  }

  if (ezstream->lqueue_omessages != nullptr) {
    ezgrpc2_message *msg;
    while ((msg = ezgrpc2_list_pop_front(ezstream->lqueue_omessages)) != nullptr) {
      ezgrpc2_message_free(msg);
    }
    ezgrpc2_list_free(ezstream->lqueue_omessages);
  }

  free(ezstream);
}
