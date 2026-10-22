#include <errno.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "core.h"
#include "ezgrpc2_header.h"
#include "ezgrpc2_server.h"
#include "ezgrpc2_session_info.h"
#include "ezgrpc2_session_uuid.h"
#include "ezgrpc2_timeout.h"
#include "internal_helpers.h"
#define atlog(...) (void)0
#define ezlog(...) (void)0

extern ezgrpc2_session_info *session_info_new(void *);

EZGRPC2_API ezgrpc2_list *ezgrpc2_server_get_all_sessions_info(ezgrpc2_server *server) {
  ezgrpc2_list *lsession_info = ezgrpc2_list_new(nullptr);
  assert(lsession_info != nullptr);

  for (EZNFDS i = 2; i < server->nb_fds; i++) {
    if (server->fds[i].fd == -1)
      continue;
    ezgrpc2_session_info *session_info = session_info_new(nullptr);
    ezgrpc2_list_push_back(lsession_info, session_info);
  }

  return lsession_info;
}

EZGRPC2_API ezgrpc2_session_info *ezgrpc2_server_get_session_info([[maybe_unused]] ezgrpc2_server *server,
                                                                 [[maybe_unused]] ezgrpc2_session_uuid *session_uuid) {
  return nullptr;
}

EZGRPC2_API ezgrpc2_server *ezgrpc2_server_new(
  const char *ipv4_addr, uint16_t ipv4_port,
  const char *ipv6_addr, uint16_t ipv6_port,
  int backlog,
  ezgrpc2_server_settings *server_settings,
  ezgrpc2_http2_settings *http2_settings) {
  struct sockaddr_in ipv4_saddr = {};
  struct sockaddr_in6 ipv6_saddr = {};
  ezgrpc2_server *server = nullptr;
  EZSOCKET ipv4_sockfd = EZINVALID_SOCKET;
  EZSOCKET ipv6_sockfd = EZINVALID_SOCKET;

  if (ipv4_addr == nullptr && ipv6_addr == nullptr)
    return nullptr;
  if (ipv4_addr != nullptr && strnlen(ipv4_addr, 16) > 15)
    return nullptr;
  if (ipv6_addr != nullptr && strnlen(ipv6_addr, 64) > 63)
    return nullptr;

  server = calloc(1, sizeof(*server));
  if (server == nullptr)
    return nullptr;

  thpool_init(&server->logger_thread, 1, 0);
  server->ipv4_addr = nullptr;
  server->ipv6_addr = nullptr;
  server->ipv4_port = ipv4_port;
  server->ipv6_port = ipv6_port;
  if (ipv4_addr != nullptr) {
    server->ipv4_addr = strndup(ipv4_addr, 16);
    assert(server->ipv4_addr != nullptr);
  }
  if (ipv6_addr != nullptr) {
    server->ipv6_addr = strndup(ipv6_addr, 64);
    assert(server->ipv6_addr != nullptr);
  }

  if (server_settings == nullptr) {
    ezgrpc2_server_settings *server_settings_ = ezgrpc2_server_settings_new(nullptr);
    assert(server_settings_ != nullptr);
    memcpy(&server->server_settings, server_settings_, sizeof(ezgrpc2_server_settings));
    ezgrpc2_server_settings_free(server_settings_);
  } else {
    memcpy(&server->server_settings, server_settings, sizeof(ezgrpc2_server_settings));
  }

  if (http2_settings == nullptr) {
    ezgrpc2_http2_settings *http2_settings_ = ezgrpc2_http2_settings_new(nullptr);
    assert(http2_settings_ != nullptr);
    memcpy(&server->http2_settings, http2_settings_, sizeof(ezgrpc2_http2_settings));
    ezgrpc2_http2_settings_free(http2_settings_);
  } else {
    memcpy(&server->http2_settings, http2_settings, sizeof(ezgrpc2_http2_settings));
  }

#ifdef _WIN32
#define perror(s) fprintf(stderr, "WSA: " s " error code: %d\n", WSAGetLastError())
#endif

  if (server->ipv4_addr != nullptr) {
    memset(&ipv4_saddr, 0, sizeof(ipv4_saddr));
    ipv4_saddr.sin_family = AF_INET;
    ipv4_saddr.sin_addr.s_addr = inet_addr(server->ipv4_addr);
    ipv4_saddr.sin_port = htons(server->ipv4_port);

    if ((ipv4_sockfd = socket(AF_INET, SOCK_STREAM, 0)) == EZINVALID_SOCKET) {
      perror("socket");
      goto err;
    }

    if (setsockopt(ipv4_sockfd, SOL_SOCKET, SO_REUSEADDR, (void*)&(int){1},
                   sizeof(int))) {
      perror("setsockopt ipv4");
      goto err;
    }

    if (bind(ipv4_sockfd, (struct sockaddr *)&ipv4_saddr, sizeof(ipv4_saddr)) == EZSOCKET_ERROR) {
      perror("bind ipv4");
      goto err;
    }

    if (listen(ipv4_sockfd, backlog) == EZSOCKET_ERROR) {
      perror("listen ipv4");
      goto err;
    }

    if (makenonblock(ipv4_sockfd)) {
      EZGRPC2_LOG_ERROR(server, "makenonblock failed\n");
      goto err;
    }

    EZGRPC2_LOG_NORMAL(server, "listening on ipv4 %s:%d ...\n", server->ipv4_addr,
          server->ipv4_port);
  }

  if (server->ipv6_addr != nullptr) {
    memset(&ipv6_saddr, 0, sizeof(ipv6_saddr));
    ipv6_saddr.sin6_family = AF_INET6;
    if (inet_pton(AF_INET6, server->ipv6_addr,
                  ipv6_saddr.sin6_addr.s6_addr) != 1) {
      perror("inet_pton");
      goto err;
    }
    ipv6_saddr.sin6_port = htons(server->ipv6_port);

    if ((ipv6_sockfd = socket(AF_INET6, SOCK_STREAM, 0)) == EZINVALID_SOCKET) {
      perror("socket(inet6,sockstream,0)");
      goto err;
    }

    if (setsockopt(ipv6_sockfd, SOL_SOCKET, SO_REUSEADDR, (void *)&(int){1},
                   sizeof(int))) {
      perror("setsockopt ipv6");
      goto err;
    }
    if (setsockopt(ipv6_sockfd, IPPROTO_IPV6, IPV6_V6ONLY, (void *)&(int){1},
                   sizeof(int))) {
      perror("setsockopt ipv6");
      goto err;
    }

    if (bind(ipv6_sockfd, (struct sockaddr *)&ipv6_saddr, sizeof(ipv6_saddr)) == EZSOCKET_ERROR) {
      perror("bind ipv6");
      goto err;
    }

    if (listen(ipv6_sockfd, 16) == EZSOCKET_ERROR) {
      perror("listen ipv6");
      goto err;
    }

    if (makenonblock(ipv6_sockfd)) {
      EZGRPC2_LOG_ERROR(server, "makenonblock failed\n");
      goto err;
    }

    EZGRPC2_LOG_NORMAL(server, "listening on ipv6 [%s]:%d ...\n", server->ipv6_addr,
          server->ipv6_port);
  }

  server->nb_sessions = server->server_settings.max_connections + 2;
  server->sessions = calloc(server->nb_sessions, sizeof(*server->sessions));

  server->nb_fds = server->server_settings.max_connections + 2;
  server->fds = calloc(server->nb_fds, sizeof(*(server->fds)));
  if (server->fds == nullptr)
    goto err;
  server->paths = calloc(server->http2_settings.max_paths, sizeof(*server->paths));
  if (server->paths == nullptr)
    goto err;

  server->fds[0].fd = ipv4_sockfd;
  server->fds[0].events = POLLIN | POLLRDHUP;
  server->fds[1].fd = ipv6_sockfd;
  server->fds[1].events = POLLIN | POLLRDHUP;

  for (EZNFDS i = 2; i < server->nb_fds; i++) {
    server->fds[i].fd = -1;
  }

  return server;

err:
  if (ipv4_sockfd != EZINVALID_SOCKET)
    close(ipv4_sockfd);
  if (ipv6_sockfd != EZINVALID_SOCKET)
    close(ipv6_sockfd);
  ezgrpc2_server_free(server);
  return nullptr;
}

EZGRPC2_API int ezgrpc2_server_poll(
  ezgrpc2_server *server,
  ezgrpc2_events *events,
  int timeout) {
  assert(events != nullptr);
  struct pollfd *fds = server->fds;

#ifdef _WIN32
  const ULONG nb_fds = server->nb_fds;
  const int ready = WSAPoll(fds, nb_fds, timeout);
#else
  const nfds_t nb_fds = server->nb_fds;
  const int ready = poll(fds, nb_fds, timeout);
#endif

  server->events = events;
  ezgrpc2_server_check_deadlines(server);

  if (ready <= 0)
    return ready;

  // add ipv4 session
  if (fds[0].revents & POLLIN)
    session_add(server, fds[0].fd);
  // add ipv6 session
  if (fds[1].revents & POLLIN)
    session_add(server, fds[1].fd);

  for (EZNFDS i = 2; i < nb_fds; i++) {
    if (fds[i].fd != -1 && (fds[i].revents & (POLLRDHUP | POLLERR))) {
      ezlog("c hangup\n");
      session_free(&server->sessions[i]);
      fds[i].fd = -1;
      fds[i].revents = 0;
      continue;
    }
    if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
      server->sessions[i].server = server;
      if (session_events(&server->sessions[i])) {
        EZGRPC2_LOG_TRACE(server, "closing session %p", (void*) &server->sessions[i]);
        if (close(server->sessions[i].sockfd)) {
          EZGRPC2_LOG_ERROR(server, "close: %s", strerror(errno));
        }
        fds[i].fd = -1;
        session_free(&server->sessions[i]);
      }
      fds[i].revents = 0;
    }
  }

  return ready;
}

EZGRPC2_API ezgrpc2_event *ezgrpc2_server_events_read([[maybe_unused]] ezgrpc2_server *server) {
  return nullptr;
}

EZGRPC2_API int ezgrpc2_server_events_poll(
  ezgrpc2_server *server,
  int timeout) {
  struct pollfd *fds = server->fds;

#ifdef _WIN32
  const ULONG nb_fds = server->nb_fds;
  const int ready = WSAPoll(fds, nb_fds, timeout);
#else
  const nfds_t nb_fds = server->nb_fds;
  const int ready = poll(fds, nb_fds, timeout);
#endif

  if (ready <= 0)
    return ready;

  // add ipv4 session
  if (fds[0].revents & POLLIN)
    session_add(server, fds[0].fd);
  // add ipv6 session
  if (fds[1].revents & POLLIN)
    session_add(server, fds[1].fd);

  for (EZNFDS i = 2; i < nb_fds; i++) {
    if (fds[i].fd != -1 && (fds[i].revents & (POLLRDHUP | POLLERR))) {
      ezlog("c hangup\n");
      session_free(&server->sessions[i]);
      fds[i].fd = -1;
      fds[i].revents = 0;
      continue;
    }
    if (fds[i].fd != -1 && (fds[i].revents & POLLIN)) {
      server->sessions[i].server = server;
      if (session_events(&server->sessions[i])) {
        EZGRPC2_LOG_TRACE(server, "closing session %p", (void*) &server->sessions[i]);
        if (close(server->sessions[i].sockfd)) {
          EZGRPC2_LOG_ERROR(server, "close: %s", strerror(errno));
        }
        fds[i].fd = -1;
        session_free(&server->sessions[i]);
      }
      fds[i].revents = 0;
    }
  }

  return ready;
}

EZGRPC2_API int ezgrpc2_server_register_path(ezgrpc2_server *server, const char *path, void *userdata, uint64_t accept_content_type, uint64_t accept_content_encoding) {
  if (server->nb_paths + 1 > server->http2_settings.max_paths)
    return 1;
  strncpy(server->paths[server->nb_paths].path, path, sizeof(server->paths[server->nb_paths].path) - 1);
  server->paths[server->nb_paths].path[sizeof(server->paths[server->nb_paths].path) - 1] = '\0';
  server->paths[server->nb_paths].userdata = userdata;
  server->paths[server->nb_paths].accept_content_type = accept_content_type;
  server->paths[server->nb_paths].accept_content_encoding = accept_content_encoding;
  server->nb_paths++;
  return 0;
}

EZGRPC2_API void *ezgrpc2_server_unregister_path(ezgrpc2_server *server, const char *path) {
  for (size_t i = 0; i < server->http2_settings.max_paths; i++) {
    if (server->paths[i].path[0] && !strcmp(path, server->paths[i].path)) {
      void *userdata = server->paths[i].userdata;
      memset(&server->paths[i], 0, sizeof(ezgrpc2_path));
      server->nb_paths--;
      return userdata;
    }
  }
  return nullptr;
}

EZGRPC2_API void ezgrpc2_server_free(ezgrpc2_server *ezserver) {
  if (ezserver == nullptr)
    return;
  for (EZNFDS i = 0; i < 2; i++) {
    if (ezserver->fds != nullptr && ezserver->fds[i].fd != -1) {
      close(ezserver->fds[i].fd);
    }
  }

  for (EZNFDS i = 2; i < ezserver->nb_fds; i++) {
    if (ezserver->fds != nullptr && ezserver->fds[i].fd != -1) {
      shutdown(ezserver->fds[i].fd, SHUT_RDWR);
      close(ezserver->fds[i].fd);
      session_free(&ezserver->sessions[i]);
    }
  }
  free(ezserver->fds);
  free(ezserver->paths);
  free(ezserver->ipv4_addr);
  free(ezserver->ipv6_addr);
  free(ezserver->sessions);
  thpool_cleanup(&ezserver->logger_thread);
  free(ezserver);
}

typedef struct log_t log_t;
struct log_t {
  char *str;
  FILE *fp;
};

static log_t *log_new(char *str, FILE *fp) {
  log_t *r = malloc(sizeof(*r));
  assert(r != nullptr);
  r->str = str;
  r->fp = fp;
  return r;
}

static void log_free(void *userdata) {
  log_t *r = userdata;
  free(r->str);
  free(r);
}

static void logger(void *userdata) {
  log_t *l = userdata;
  char dt[128] = {};
  fprintf(l->fp, "[%s] ", ezgetdt(dt, sizeof(dt)));
  fwrite(l->str, 1, strlen(l->str), l->fp);
  log_free(l);
}

void ezgrpc2_server_log(ezgrpc2_server *server, uint32_t log_level, const char *fmt, ...) {
  if (!(log_level & EZGRPC2_SERVER_LOG_NORMAL || log_level & server->server_settings.logging_level))
    return;
  va_list ap;
  va_start(ap, fmt);
  char *buf = malloc(1024);
  assert(buf != nullptr);
  vsnprintf(buf, 1024, fmt, ap);
  log_t *l = log_new(buf, server->server_settings.logging_fp);
  thpool_add_task(&server->logger_thread, logger, l, log_free); 
  va_end(ap);
}

EZGRPC2_API ezgrpc2_server_settings *ezgrpc2_server_get_server_settings(ezgrpc2_server *server) {
  return server ? &server->server_settings : nullptr;
}

EZGRPC2_API ezgrpc2_http2_settings *ezgrpc2_server_get_http2_settings(ezgrpc2_server *server) {
  return server ? &server->http2_settings : nullptr;
}
