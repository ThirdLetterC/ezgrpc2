/* The TLS backend independent part: the public API and the per-session
 * plumbing. The TLS library itself is driven by src/ezgrpc2_tls_<backend>.c */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"
#include "internal_tls.h"

#if defined(EZGRPC2_ENABLE_TLS)

EZGRPC2_API const char *ezgrpc2_tls_backend(void) {
  return tls_backend_name();
}

EZGRPC2_API const char *ezgrpc2_tls_backend_version(void) {
  return tls_backend_version();
}

EZGRPC2_API void *ezgrpc2_server_tls_native(ezgrpc2_server *server) {
  return server != nullptr && server->tls_ctx != nullptr ? tls_backend_ctx_native(server->tls_ctx)
                                                         : nullptr;
}

ezgrpc2_err tls_enable(ezgrpc2_server *server, const char *cert_file, const char *key_file) {
  ezgrpc2_tls_ctx *ctx = tls_backend_ctx_new(cert_file, key_file);
  if (ctx == nullptr) {
    LOG_ERROR(server, "cannot load the TLS certificate '%s' and key '%s'\n", cert_file, key_file);
    return EZGRPC2_ERR_INVALID;
  }
  if (server->tls_ctx != nullptr)
    tls_backend_ctx_free(server->tls_ctx);
  server->tls_ctx = ctx;
  server->is_tls = 1;
  return EZGRPC2_OK;
}

void tls_free(ezgrpc2_server *server) {
  if (server->tls_ctx != nullptr) {
    tls_backend_ctx_free(server->tls_ctx);
    server->tls_ctx = nullptr;
  }
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

EZGRPC2_API const char *ezgrpc2_tls_backend(void) {
  return "none";
}

EZGRPC2_API const char *ezgrpc2_tls_backend_version(void) {
  return "";
}

EZGRPC2_API void *ezgrpc2_server_tls_native([[maybe_unused]] ezgrpc2_server *server) {
  return nullptr;
}

ezgrpc2_err tls_enable(ezgrpc2_server *server, [[maybe_unused]] const char *cert_file,
                       [[maybe_unused]] const char *key_file) {
  LOG_ERROR(server, "TLS is configured, but ezgrpc2 is built without TLS (TLS=none)\n");
  return EZGRPC2_ERR_UNSUPPORTED;
}

void tls_free([[maybe_unused]] ezgrpc2_server *server) {}

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
