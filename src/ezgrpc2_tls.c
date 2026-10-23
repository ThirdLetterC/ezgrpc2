/* The TLS backend independent part: the public API and the per-session
 * plumbing. The TLS library itself is driven by src/ezgrpc2_tls_<backend>.c */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ezgrpc2_tls.h"
#include "ezgrpc2_server.h"
#include "core.h"
#include "internal_tls.h"

#if defined(EZGRPC2_ENABLE_TLS)

EZGRPC2_API ezgrpc2_tls_ctx *ezgrpc2_tls_init_server_ctx(const char *cert_file,
                                                         const char *key_file) {
  if (cert_file == nullptr || key_file == nullptr) {
    return nullptr;
  }
  return tls_backend_ctx_new(cert_file, key_file);
}

EZGRPC2_API void ezgrpc2_tls_free_server_ctx(ezgrpc2_tls_ctx *ctx) {
  if (ctx != nullptr) {
    tls_backend_ctx_free(ctx);
  }
}

EZGRPC2_API void *ezgrpc2_tls_ctx_native(ezgrpc2_tls_ctx *ctx) {
  return ctx != nullptr ? tls_backend_ctx_native(ctx) : nullptr;
}

EZGRPC2_API const char *ezgrpc2_tls_backend(void) {
  return tls_backend_name();
}

EZGRPC2_API const char *ezgrpc2_tls_backend_version(void) {
  return tls_backend_version();
}

EZGRPC2_API int ezgrpc2_server_enable_tls(ezgrpc2_server *server, const char *cert_file,
                                          const char *key_file) {
  if (server == nullptr || cert_file == nullptr || key_file == nullptr) {
    return 1;
  }

  ezgrpc2_tls_ctx *ctx = ezgrpc2_tls_init_server_ctx(cert_file, key_file);
  if (ctx == nullptr) {
    return 2;
  }

  ezgrpc2_tls_free_server_ctx(server->tls_ctx);
  server->tls_ctx = ctx;
  server->is_tls = 1;
  return 0;
}

int ezgrpc2_tls_session_init(ezgrpc2_session *session) {
  if (session == nullptr || session->server == nullptr || !session->server->is_tls ||
      session->server->tls_ctx == nullptr) {
    return 0;
  }

  ezgrpc2_tls_session *tls = tls_backend_session_new(session->server->tls_ctx);
  if (tls == nullptr) {
    return 1;
  }

  session->tls = tls;
  session->is_tls = 1;
  session->tls_handshake_done = 0;
  return 0;
}

int ezgrpc2_tls_feed(ezgrpc2_session *session, const void *buf, size_t len) {
  if (session == nullptr || session->tls == nullptr) {
    return 1;
  }
  return tls_backend_feed(session->tls, buf, len);
}

int ezgrpc2_tls_drain(ezgrpc2_session *session, ezgrpc2_buf *out) {
  if (session == nullptr || session->tls == nullptr) {
    return 1;
  }
  return tls_backend_drain(session->tls, out);
}

int ezgrpc2_tls_handshake(ezgrpc2_session *session) {
  if (session == nullptr || session->tls == nullptr) {
    return -1;
  }
  if (session->tls_handshake_done) {
    return 1;
  }

  int res = tls_backend_handshake(session->tls);
  if (res == 1) {
    session->tls_handshake_done = 1;
  }
  return res;
}

ssize_t ezgrpc2_tls_read(ezgrpc2_session *session, void *buf, size_t len) {
  if (session == nullptr || session->tls == nullptr) {
    return -1;
  }
  return tls_backend_read(session->tls, buf, len);
}

ssize_t ezgrpc2_tls_write(ezgrpc2_session *session, const void *buf, size_t len) {
  if (session == nullptr || session->tls == nullptr) {
    return -1;
  }
  return tls_backend_write(session->tls, buf, len);
}

void ezgrpc2_tls_close_notify(ezgrpc2_session *session) {
  if (session != nullptr && session->tls != nullptr) {
    tls_backend_close_notify(session->tls);
  }
}

void ezgrpc2_tls_session_free(ezgrpc2_session *session) {
  if (session != nullptr && session->tls != nullptr) {
    tls_backend_session_free(session->tls);
    session->tls = nullptr;
    session->is_tls = 0;
    session->tls_handshake_done = 0;
  }
}

#else /* built without TLS */

EZGRPC2_API ezgrpc2_tls_ctx *ezgrpc2_tls_init_server_ctx([[maybe_unused]] const char *cert_file,
                                                         [[maybe_unused]] const char *key_file) {
  return nullptr;
}

EZGRPC2_API void ezgrpc2_tls_free_server_ctx([[maybe_unused]] ezgrpc2_tls_ctx *ctx) {}

EZGRPC2_API void *ezgrpc2_tls_ctx_native([[maybe_unused]] ezgrpc2_tls_ctx *ctx) {
  return nullptr;
}

EZGRPC2_API const char *ezgrpc2_tls_backend(void) {
  return "none";
}

EZGRPC2_API const char *ezgrpc2_tls_backend_version(void) {
  return "";
}

EZGRPC2_API int ezgrpc2_server_enable_tls([[maybe_unused]] ezgrpc2_server *server,
                                          [[maybe_unused]] const char *cert_file,
                                          [[maybe_unused]] const char *key_file) {
  return -1;
}

int ezgrpc2_tls_session_init([[maybe_unused]] ezgrpc2_session *session) {
  return 0;
}

int ezgrpc2_tls_feed([[maybe_unused]] ezgrpc2_session *session, [[maybe_unused]] const void *buf,
                     [[maybe_unused]] size_t len) {
  return 1;
}

int ezgrpc2_tls_drain([[maybe_unused]] ezgrpc2_session *session,
                      [[maybe_unused]] ezgrpc2_buf *out) {
  return 1;
}

int ezgrpc2_tls_handshake([[maybe_unused]] ezgrpc2_session *session) {
  return -1;
}

ssize_t ezgrpc2_tls_read([[maybe_unused]] ezgrpc2_session *session, [[maybe_unused]] void *buf,
                         [[maybe_unused]] size_t len) {
  return -1;
}

ssize_t ezgrpc2_tls_write([[maybe_unused]] ezgrpc2_session *session,
                          [[maybe_unused]] const void *buf, [[maybe_unused]] size_t len) {
  return -1;
}

void ezgrpc2_tls_close_notify([[maybe_unused]] ezgrpc2_session *session) {}

void ezgrpc2_tls_session_free([[maybe_unused]] ezgrpc2_session *session) {}

#endif

EZGRPC2_API ezgrpc2_server *ezgrpc2_server_new_tls(const char *ipv4_addr, uint16_t ipv4_port,
                                                   const char *ipv6_addr, uint16_t ipv6_port,
                                                   int backlog, const char *cert_file,
                                                   const char *key_file,
                                                   ezgrpc2_server_settings *server_settings,
                                                   ezgrpc2_http2_settings *http2_settings) {
  ezgrpc2_server *server = ezgrpc2_server_new(ipv4_addr, ipv4_port, ipv6_addr, ipv6_port, backlog,
                                              server_settings, http2_settings);
  if (server == nullptr) {
    return nullptr;
  }

  if (ezgrpc2_server_enable_tls(server, cert_file, key_file) != 0) {
    ezgrpc2_server_free(server);
    return nullptr;
  }

  return server;
}
