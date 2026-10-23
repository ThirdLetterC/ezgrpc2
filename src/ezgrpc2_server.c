#include <errno.h>
#include <stdarg.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <time.h>

#include "core.h"
#include "ezgrpc2_timeout.h"
#include "internal_helpers.h"
#include "internal_nghttp2_callbacks.h"

#define DEFAULT_BACKLOG                1024
#define DEFAULT_MAX_CONNECTIONS        1024
#define DEFAULT_MAX_CONCURRENT_STREAMS 1024
#define DEFAULT_MAX_MESSAGE_SIZE       (1u << 20)
#define DEFAULT_MAX_FRAME_SIZE         (16u * 1024)

/* ---- logging ------------------------------------------------------------------ */

void server_log(ezgrpc2_server *server, ezgrpc2_log_level level, const char *fmt, ...) {
  if (server->log_level == EZGRPC2_LOG_OFF || level > server->log_level)
    return;

  /* one write per line, so that lines of several servers do not interleave */
  char line[1024];
  char dt[128] = {};
  int n = snprintf(line, sizeof(line), "[%s] ", ezgetdt(dt, sizeof(dt)));
  va_list ap;
  va_start(ap, fmt);
  if (n >= 0 && (size_t)n < sizeof(line))
    vsnprintf(line + n, sizeof(line) - (size_t)n, fmt, ap);
  va_end(ap);
  fputs(line, server->log_file);
}

/* ---- routes ------------------------------------------------------------------- */

ezgrpc2_route *server_find_route(ezgrpc2_server *server, const char *path, size_t len) {
  for (size_t i = 0; i < server->nb_routes; i++) {
    ezgrpc2_route *route = server->routes[i];
    if (route->path_len == len && memcmp(route->path, path, len) == 0)
      return route;
  }
  return nullptr;
}

ezgrpc2_err server_add_route(ezgrpc2_server *server, const char *path, route_kind kind,
                             ezgrpc2_handler_fn handler, const ezgrpc2_stream_handler *stream,
                             void *userdata) {
  if (server == nullptr || path == nullptr || path[0] != '/')
    return EZGRPC2_ERR_INVALID;

  size_t len = strlen(path);
  ezgrpc2_route *route = server_find_route(server, path, len);
  if (route != nullptr) {
    if (!route->disabled)
      return EZGRPC2_ERR_EXISTS;
  } else {
    /* routes are never moved: calls point to them */
    if (server->nb_routes == server->cap_routes) {
      size_t cap = server->cap_routes ? server->cap_routes * 2 : 16;
      ezgrpc2_route **routes = ezgrpc2_realloc(server->routes, cap * sizeof(*routes));
      if (routes == nullptr)
        return EZGRPC2_ERR_NOMEM;
      server->routes = routes;
      server->cap_routes = cap;
    }
    route = ezgrpc2_calloc(1, sizeof(*route));
    if (route == nullptr)
      return EZGRPC2_ERR_NOMEM;
    route->path = ezgrpc2_strndup(path, len);
    if (route->path == nullptr) {
      ezgrpc2_free(route);
      return EZGRPC2_ERR_NOMEM;
    }
    route->path_len = len;
    server->routes[server->nb_routes++] = route;
  }

  route->kind = kind;
  route->handler = handler;
  route->stream = stream != nullptr ? *stream : (ezgrpc2_stream_handler){};
  route->userdata = userdata;
  route->disabled = false;
  return EZGRPC2_OK;
}

EZGRPC2_API ezgrpc2_err ezgrpc2_server_handle(ezgrpc2_server *server, const char *path,
                                              ezgrpc2_handler_fn handler, void *userdata) {
  if (handler == nullptr)
    return EZGRPC2_ERR_INVALID;
  return server_add_route(server, path, ROUTE_UNARY, handler, nullptr, userdata);
}

EZGRPC2_API ezgrpc2_err ezgrpc2_server_handle_stream(ezgrpc2_server *server, const char *path,
                                                     const ezgrpc2_stream_handler *handler,
                                                     void *userdata) {
  if (handler == nullptr || handler->on_message == nullptr)
    return EZGRPC2_ERR_INVALID;
  return server_add_route(server, path, ROUTE_STREAM, nullptr, handler, userdata);
}

EZGRPC2_API ezgrpc2_err ezgrpc2_server_register_path(ezgrpc2_server *server, const char *path,
                                                     void *userdata) {
  return server_add_route(server, path, ROUTE_EVENTS, nullptr, nullptr, userdata);
}

EZGRPC2_API ezgrpc2_err ezgrpc2_server_unregister(ezgrpc2_server *server, const char *path) {
  if (server == nullptr || path == nullptr)
    return EZGRPC2_ERR_INVALID;
  ezgrpc2_route *route = server_find_route(server, path, strlen(path));
  if (route == nullptr || route->disabled)
    return EZGRPC2_ERR_NOT_FOUND;
  /* kept, since calls in progress point to it */
  route->disabled = true;
  return EZGRPC2_OK;
}

/* ---- setup ---------------------------------------------------------------------- */

/* binds and listens on one address. returns the socket or -1 */
static int open_listener(ezgrpc2_server *server, int family, const char *addr, uint16_t port,
                         int backlog) {
  struct sockaddr_storage ss = {};
  socklen_t sslen;
  if (family == AF_INET) {
    struct sockaddr_in *sin = (struct sockaddr_in *)&ss;
    sin->sin_family = AF_INET;
    sin->sin_port = htons(port);
    if (inet_pton(AF_INET, addr, &sin->sin_addr) != 1) {
      LOG_ERROR(server, "invalid IPv4 address '%s'\n", addr);
      return -1;
    }
    sslen = sizeof(*sin);
  } else {
    struct sockaddr_in6 *sin6 = (struct sockaddr_in6 *)&ss;
    sin6->sin6_family = AF_INET6;
    sin6->sin6_port = htons(port);
    if (inet_pton(AF_INET6, addr, &sin6->sin6_addr) != 1) {
      LOG_ERROR(server, "invalid IPv6 address '%s'\n", addr);
      return -1;
    }
    sslen = sizeof(*sin6);
  }

  int fd = socket(family, SOCK_STREAM | SOCK_CLOEXEC, 0);
  if (fd == -1) {
    LOG_ERROR(server, "socket: %s\n", strerror(errno));
    return -1;
  }
  (void)setsockopt(fd, SOL_SOCKET, SO_REUSEADDR, &(int){1}, sizeof(int));
  /* lets one server per thread share the port; the kernel spreads connections */
  (void)setsockopt(fd, SOL_SOCKET, SO_REUSEPORT, &(int){1}, sizeof(int));
  if (family == AF_INET6)
    (void)setsockopt(fd, IPPROTO_IPV6, IPV6_V6ONLY, &(int){1}, sizeof(int));

  if (bind(fd, (struct sockaddr *)&ss, sslen) == -1) {
    LOG_ERROR(server, "bind %s port %u: %s\n", addr, port, strerror(errno));
    close(fd);
    return -1;
  }
  if (listen(fd, backlog) == -1) {
    LOG_ERROR(server, "listen %s port %u: %s\n", addr, port, strerror(errno));
    close(fd);
    return -1;
  }
  return fd;
}

/* hands an open socket to an initialized libuv listener, which then owns it */
static bool start_listener(ezgrpc2_server *server, uv_tcp_t *listener, int *fd) {
  listener->data = server;
  if (uv_tcp_open(listener, *fd) != 0)
    return false;
  *fd = -1;
  return uv_listen((uv_stream_t *)listener, server->config.backlog, on_listener_connection) == 0;
}

static void on_poll_timer([[maybe_unused]] uv_timer_t *handle) {}

static void run_posted(ezgrpc2_server *server) {
  pthread_mutex_lock(&server->post_mutex);
  post_task *task = server->post_head;
  server->post_head = server->post_tail = nullptr;
  pthread_mutex_unlock(&server->post_mutex);

  while (task != nullptr) {
    post_task *next = task->next;
    task->fn(task->arg);
    ezgrpc2_free(task);
    task = next;
  }
}

static void on_async(uv_async_t *handle) {
  ezgrpc2_server *server = handle->data;
  server->ready_events++;
  run_posted(server);
}

EZGRPC2_API ezgrpc2_server *ezgrpc2_server_new(const ezgrpc2_config *config) {
  if (config == nullptr || config->port == 0) {
    fprintf(config != nullptr && config->log_file != nullptr ? config->log_file : stderr,
            "ezgrpc2_server_new: a port is required\n");
    return nullptr;
  }

  ezgrpc2_server *server = ezgrpc2_calloc(1, sizeof(*server));
  if (server == nullptr)
    return nullptr;
  server->ipv4_sockfd = -1;
  server->ipv6_sockfd = -1;
  server->next_deadline_ms = UINT64_MAX;
  pthread_mutex_init(&server->post_mutex, nullptr);

  /* the configuration, with defaults */
  ezgrpc2_config *cfg = &server->config;
  *cfg = *config;
  if (cfg->backlog <= 0)
    cfg->backlog = DEFAULT_BACKLOG;
  if (cfg->max_connections == 0)
    cfg->max_connections = DEFAULT_MAX_CONNECTIONS;
  if (cfg->max_concurrent_streams == 0)
    cfg->max_concurrent_streams = DEFAULT_MAX_CONCURRENT_STREAMS;
  if (cfg->max_message_size == 0)
    cfg->max_message_size = DEFAULT_MAX_MESSAGE_SIZE;
  if (cfg->max_frame_size == 0)
    cfg->max_frame_size = DEFAULT_MAX_FRAME_SIZE;
  server->log_level = cfg->log_level == EZGRPC2_LOG_DEFAULT ? EZGRPC2_LOG_INFO : cfg->log_level;
  server->log_file = cfg->log_file != nullptr ? cfg->log_file : stderr;
  cfg->log_file = server->log_file;

  /* HTTP/2 limits: a frame is 16 KiB to 16 MiB, a window at most 2^31 - 1 */
  if (cfg->max_frame_size < (16u * 1024) || cfg->max_frame_size > (1u << 24) - 1 ||
      cfg->max_message_size > INT32_MAX) {
    LOG_ERROR(server, "max_frame_size or max_message_size is out of range\n");
    goto err;
  }

  const char *ipv4_addr = cfg->ipv4_addr, *ipv6_addr = cfg->ipv6_addr;
  if (ipv4_addr == nullptr && ipv6_addr == nullptr) {
    ipv4_addr = "0.0.0.0";
    ipv6_addr = "::";
  }
  if ((ipv4_addr != nullptr && (server->ipv4_addr = ezgrpc2_strdup(ipv4_addr)) == nullptr) ||
      (ipv6_addr != nullptr && (server->ipv6_addr = ezgrpc2_strdup(ipv6_addr)) == nullptr))
    goto err;
  /* the copies outlive the caller's strings */
  cfg->ipv4_addr = server->ipv4_addr;
  cfg->ipv6_addr = server->ipv6_addr;
  cfg->tls.cert_file = cfg->tls.key_file = nullptr;

  if ((config->tls.cert_file == nullptr) != (config->tls.key_file == nullptr)) {
    LOG_ERROR(server, "TLS needs both tls.cert_file and tls.key_file\n");
    goto err;
  }
  if (config->tls.cert_file != nullptr &&
      tls_enable(server, config->tls.cert_file, config->tls.key_file) != EZGRPC2_OK)
    goto err;

  if (server->ipv4_addr != nullptr &&
      (server->ipv4_sockfd = open_listener(server, AF_INET, server->ipv4_addr, cfg->port,
                                           cfg->backlog)) == -1)
    goto err;
  if (server->ipv6_addr != nullptr &&
      (server->ipv6_sockfd = open_listener(server, AF_INET6, server->ipv6_addr, cfg->port,
                                           cfg->backlog)) == -1)
    goto err;

  if (uv_loop_init(&server->loop) != 0)
    goto err;
  server->loop_initialized = true;

  if (uv_timer_init(&server->loop, &server->timer) != 0)
    goto err;
  server->timer_active = true;
  if (uv_timer_init(&server->loop, &server->deadline_timer) != 0)
    goto err;
  server->deadline_timer.data = server;
  server->deadline_timer_active = true;
  if (uv_async_init(&server->loop, &server->async, on_async) != 0)
    goto err;
  server->async.data = server;
  server->async_active = true;

  if (server->ipv4_sockfd != -1) {
    if (uv_tcp_init(&server->loop, &server->ipv4_listener) != 0)
      goto err;
    server->ipv4_listener_active = true;
    if (!start_listener(server, &server->ipv4_listener, &server->ipv4_sockfd))
      goto err;
  }
  if (server->ipv6_sockfd != -1) {
    if (uv_tcp_init(&server->loop, &server->ipv6_listener) != 0)
      goto err;
    server->ipv6_listener_active = true;
    if (!start_listener(server, &server->ipv6_listener, &server->ipv6_sockfd))
      goto err;
  }

  if (nghttp2_session_callbacks_new(&server->ngcallbacks) != 0)
    goto err;
  server_setup_ngcallbacks(server->ngcallbacks);

  server->rbuf_len = 64 * 1024;
  server->rbuf = ezgrpc2_malloc(server->rbuf_len);
  if (server->rbuf == nullptr)
    goto err;

  server->nb_sessions = cfg->max_connections;
  server->sessions = ezgrpc2_calloc(server->nb_sessions, sizeof(*server->sessions));
  server->free_sessions = ezgrpc2_malloc(server->nb_sessions * sizeof(*server->free_sessions));
  if (server->sessions == nullptr || server->free_sessions == nullptr)
    goto err;
  /* lowest index on top */
  for (size_t i = 0; i < server->nb_sessions; i++)
    server->free_sessions[i] = server->nb_sessions - 1 - i;
  server->nb_free_sessions = server->nb_sessions;

  if (server->ipv4_addr != nullptr)
    LOG_INFO(server, "listening on %s:%u%s\n", server->ipv4_addr, cfg->port,
             server->is_tls ? " (TLS)" : "");
  if (server->ipv6_addr != nullptr)
    LOG_INFO(server, "listening on [%s]:%u%s\n", server->ipv6_addr, cfg->port,
             server->is_tls ? " (TLS)" : "");
  return server;

err:
  ezgrpc2_server_free(server);
  return nullptr;
}

EZGRPC2_API void ezgrpc2_server_free(ezgrpc2_server *server) {
  if (server == nullptr)
    return;

  if (server->sessions != nullptr) {
    /* deliver what the application already queued */
    server_flush_sessions(server);
    for (size_t i = 0; i < server->nb_sessions; i++) {
      if (server->sessions[i].is_active)
        session_close(&server->sessions[i]);
    }
  }
  /* posted work runs (on calls that are now closed) so that it releases its references */
  run_posted(server);
  server_drop_ready(server);
  server_clear_events(server);

  if (server->ipv4_listener_active)
    uv_close((uv_handle_t *)&server->ipv4_listener, nullptr);
  if (server->ipv6_listener_active)
    uv_close((uv_handle_t *)&server->ipv6_listener, nullptr);
  if (server->timer_active)
    uv_close((uv_handle_t *)&server->timer, nullptr);
  if (server->deadline_timer_active)
    uv_close((uv_handle_t *)&server->deadline_timer, nullptr);
  if (server->async_active)
    uv_close((uv_handle_t *)&server->async, nullptr);
  if (server->ipv4_sockfd != -1)
    close(server->ipv4_sockfd);
  if (server->ipv6_sockfd != -1)
    close(server->ipv6_sockfd);

  if (server->loop_initialized) {
    int drain_limit = 1000;
    while (uv_loop_alive(&server->loop) && drain_limit-- > 0)
      uv_run(&server->loop, UV_RUN_NOWAIT);
    uv_loop_close(&server->loop);
  }

  for (size_t i = 0; i < server->nb_routes; i++) {
    ezgrpc2_free(server->routes[i]->path);
    ezgrpc2_free(server->routes[i]);
  }
  ezgrpc2_free(server->routes);
  interceptors_free(server);
  health_free(server);
  reflection_free(server);
  tls_free(server);

  if (server->ngcallbacks != nullptr)
    nghttp2_session_callbacks_del(server->ngcallbacks);
  ezgrpc2_free(server->events);
  ezgrpc2_free(server->rbuf);
  ezgrpc2_free(server->tls_obuf.data);
  ezgrpc2_free(server->ipv4_addr);
  ezgrpc2_free(server->ipv6_addr);
  ezgrpc2_free(server->sessions);
  ezgrpc2_free(server->free_sessions);
  pthread_mutex_destroy(&server->post_mutex);
  ezgrpc2_free(server);
}

/* ---- running ---------------------------------------------------------------------- */

EZGRPC2_API int ezgrpc2_server_poll(ezgrpc2_server *server, int timeout_ms) {
  if (server == nullptr)
    return EZGRPC2_ERR_INVALID;

  server_clear_events(server);
  server->ready_events = 0;

  /* write everything queued since the last poll, one write per connection */
  server_flush_sessions(server);

  if (timeout_ms > 0) {
    uv_timer_start(&server->timer, on_poll_timer, (uint64_t)timeout_ms, 0);
    uv_run(&server->loop, UV_RUN_ONCE);
    uv_timer_stop(&server->timer);
  } else if (timeout_ms == 0) {
    uv_run(&server->loop, UV_RUN_NOWAIT);
  } else {
    uv_run(&server->loop, UV_RUN_ONCE);
  }

  server_dispatch(server);
  /* what the handlers answered */
  server_flush_sessions(server);
  return (int)server->nb_events;
}

EZGRPC2_API const ezgrpc2_event *ezgrpc2_server_events(ezgrpc2_server *server, size_t *count) {
  if (server == nullptr) {
    if (count != nullptr)
      *count = 0;
    return nullptr;
  }
  if (count != nullptr)
    *count = server->nb_events;
  return server->events;
}

EZGRPC2_API ezgrpc2_err ezgrpc2_server_run(ezgrpc2_server *server) {
  if (server == nullptr)
    return EZGRPC2_ERR_INVALID;
  while (!atomic_load(&server->stop)) {
    int res = ezgrpc2_server_poll(server, -1);
    if (res < 0)
      return (ezgrpc2_err)res;
  }
  /* the server can be run again */
  atomic_store(&server->stop, false);
  return EZGRPC2_OK;
}

EZGRPC2_API void ezgrpc2_server_stop(ezgrpc2_server *server) {
  if (server == nullptr)
    return;
  atomic_store(&server->stop, true);
  /* async-signal-safe */
  uv_async_send(&server->async);
}

EZGRPC2_API ezgrpc2_err ezgrpc2_server_post(ezgrpc2_server *server, void (*fn)(void *arg),
                                            void *arg) {
  if (server == nullptr || fn == nullptr)
    return EZGRPC2_ERR_INVALID;
  post_task *task = ezgrpc2_malloc(sizeof(*task));
  if (task == nullptr)
    return EZGRPC2_ERR_NOMEM;
  *task = (post_task){.fn = fn, .arg = arg};

  pthread_mutex_lock(&server->post_mutex);
  if (server->post_tail != nullptr)
    server->post_tail->next = task;
  else
    server->post_head = task;
  server->post_tail = task;
  pthread_mutex_unlock(&server->post_mutex);

  uv_async_send(&server->async);
  return EZGRPC2_OK;
}

EZGRPC2_API void ezgrpc2_server_flush(ezgrpc2_server *server) {
  if (server != nullptr)
    server_flush_sessions(server);
}

EZGRPC2_API const char *ezgrpc2_license(void) {
  static const char *license =
      "ezgrpc2 - A grpc server without the extra fancy features.\n"
      "https://github.com/mnyoshie/ezgrpc2\n"
      "\n"
      "Copyright (c) 2023-2025 M. N. Yoshie & Al-buharie Amjari\n"
      "\n"
      "Redistribution and use in source and binary forms, with or without\n"
      "modification, are permitted provided that the following conditions\n"
      "are met:\n"
      "\n"
      "  1. Redistributions of source code must retain the above copyright notice,\n"
      "  this list of conditions and the following disclaimer.\n"
      "\n"
      "  2. Neither the name of the copyright holder nor the names of its\n"
      "  contributors may be used to endorse or promote products derived from\n"
      "  this software without specific prior written permission.\n"
      "\n"
      "THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS\n"
      "\"AS IS\" AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT\n"
      "LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR\n"
      "A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT\n"
      "HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,\n"
      "SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED\n"
      "TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR\n"
      "PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF\n"
      "LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING\n"
      "NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS\n"
      "SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.\n";
  return license;
}
