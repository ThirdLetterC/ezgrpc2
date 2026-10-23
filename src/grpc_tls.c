/* The TLS backend independent part: the public API and the per-session
 * plumbing. The TLS library itself is driven by src/grpc_tls_<backend>.c */
#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "core.h"
#include "internal_tls.h"

#if defined(GRPC_ENABLE_TLS)

GRPC_API const char *grpc_tls_backend(void) {
  return tls_backend_name();
}

GRPC_API const char *grpc_tls_backend_version(void) {
  return tls_backend_version();
}

GRPC_API void *grpc_server_tls_native(grpc_server *server) {
  return server != nullptr && server->tls_ctx != nullptr ? tls_backend_ctx_native(server->tls_ctx)
                                                         : nullptr;
}

grpc_err tls_enable(grpc_server *server, const char *cert_file, const char *key_file) {
  grpc_tls_ctx *ctx = tls_backend_ctx_new(cert_file, key_file);
  if (ctx == nullptr) {
    LOG_ERROR(server, "cannot load the TLS certificate '%s' and key '%s'\n", cert_file, key_file);
    return GRPC_ERR_INVALID;
  }
  if (server->tls_ctx != nullptr)
    tls_backend_ctx_free(server->tls_ctx);
  server->tls_ctx = ctx;
  server->is_tls = 1;
  return GRPC_OK;
}

void tls_free(grpc_server *server) {
  if (server->tls_ctx != nullptr) {
    tls_backend_ctx_free(server->tls_ctx);
    server->tls_ctx = nullptr;
  }
}

int grpc_tls_session_init(grpc_session *session) {
  if (session == nullptr || session->server == nullptr || !session->server->is_tls ||
      session->server->tls_ctx == nullptr) {
    return 0;
  }

  grpc_tls_session *tls = tls_backend_session_new(session->server->tls_ctx);
  if (tls == nullptr) {
    return 1;
  }

  session->tls = tls;
  session->is_tls = 1;
  session->tls_handshake_done = 0;
  return 0;
}

int grpc_tls_feed(grpc_session *session, const void *buf, size_t len) {
  if (session == nullptr || session->tls == nullptr) {
    return 1;
  }
  return tls_backend_feed(session->tls, buf, len);
}

int grpc_tls_drain(grpc_session *session, grpc_buf *out) {
  if (session == nullptr || session->tls == nullptr) {
    return 1;
  }
  return tls_backend_drain(session->tls, out);
}

int grpc_tls_handshake(grpc_session *session) {
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

ssize_t grpc_tls_read(grpc_session *session, void *buf, size_t len) {
  if (session == nullptr || session->tls == nullptr) {
    return -1;
  }
  return tls_backend_read(session->tls, buf, len);
}

ssize_t grpc_tls_write(grpc_session *session, const void *buf, size_t len) {
  if (session == nullptr || session->tls == nullptr) {
    return -1;
  }
  return tls_backend_write(session->tls, buf, len);
}

void grpc_tls_close_notify(grpc_session *session) {
  if (session != nullptr && session->tls != nullptr) {
    tls_backend_close_notify(session->tls);
  }
}

void grpc_tls_session_free(grpc_session *session) {
  if (session != nullptr && session->tls != nullptr) {
    tls_backend_session_free(session->tls);
    session->tls = nullptr;
    session->is_tls = 0;
    session->tls_handshake_done = 0;
  }
}

#else /* built without TLS */

GRPC_API const char *grpc_tls_backend(void) {
  return "none";
}

GRPC_API const char *grpc_tls_backend_version(void) {
  return "";
}

GRPC_API void *grpc_server_tls_native([[maybe_unused]] grpc_server *server) {
  return nullptr;
}

grpc_err tls_enable(grpc_server *server, [[maybe_unused]] const char *cert_file,
                       [[maybe_unused]] const char *key_file) {
  LOG_ERROR(server, "TLS is configured, but grpc is built without TLS (TLS=none)\n");
  return GRPC_ERR_UNSUPPORTED;
}

void tls_free([[maybe_unused]] grpc_server *server) {}

int grpc_tls_session_init([[maybe_unused]] grpc_session *session) {
  return 0;
}

int grpc_tls_feed([[maybe_unused]] grpc_session *session, [[maybe_unused]] const void *buf,
                     [[maybe_unused]] size_t len) {
  return 1;
}

int grpc_tls_drain([[maybe_unused]] grpc_session *session,
                      [[maybe_unused]] grpc_buf *out) {
  return 1;
}

int grpc_tls_handshake([[maybe_unused]] grpc_session *session) {
  return -1;
}

ssize_t grpc_tls_read([[maybe_unused]] grpc_session *session, [[maybe_unused]] void *buf,
                         [[maybe_unused]] size_t len) {
  return -1;
}

ssize_t grpc_tls_write([[maybe_unused]] grpc_session *session,
                          [[maybe_unused]] const void *buf, [[maybe_unused]] size_t len) {
  return -1;
}

void grpc_tls_close_notify([[maybe_unused]] grpc_session *session) {}

void grpc_tls_session_free([[maybe_unused]] grpc_session *session) {}

#endif
