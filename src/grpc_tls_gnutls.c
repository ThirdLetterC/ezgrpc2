/* TLS backend for GnuTLS. The transport push/pull functions move ciphertext
 * through grpc_tls_io. */
#include <errno.h>
#include <gnutls/gnutls.h>
#include "core.h"
#include "internal_tls.h"

struct grpc_tls_ctx {
  gnutls_certificate_credentials_t cred;
  gnutls_priority_t priority;
};

struct grpc_tls_session {
  gnutls_session_t session;
  grpc_tls_io io;
};

const char *tls_backend_name(void) {
  return "gnutls";
}

const char *tls_backend_version(void) {
  return gnutls_check_version(nullptr);
}

static ssize_t io_pull(gnutls_transport_ptr_t ptr, void *buf, size_t len) {
  grpc_tls_session *tls = ptr;
  size_t n = tls_io_pull(&tls->io, buf, len);
  if (n == 0) {
    gnutls_transport_set_errno(tls->session, EAGAIN);
    return -1;
  }
  return (ssize_t)n;
}

static int io_pull_timeout(gnutls_transport_ptr_t ptr, [[maybe_unused]] unsigned int ms) {
  grpc_tls_session *tls = ptr;
  return tls_io_pending(&tls->io) > 0 ? 1 : 0;
}

static ssize_t io_push(gnutls_transport_ptr_t ptr, const void *buf, size_t len) {
  grpc_tls_session *tls = ptr;
  if (tls_io_push(&tls->io, buf, len)) {
    gnutls_transport_set_errno(tls->session, ENOMEM);
    return -1;
  }
  return (ssize_t)len;
}

grpc_tls_ctx *tls_backend_ctx_new(const char *cert_file, const char *key_file) {
  grpc_tls_ctx *ctx = grpc_calloc(1, sizeof(*ctx));
  if (ctx == nullptr) {
    return nullptr;
  }
  /* the key file is checked against the certificate here as well */
  if (gnutls_certificate_allocate_credentials(&ctx->cred) < 0 ||
      gnutls_certificate_set_x509_key_file(ctx->cred, cert_file, key_file, GNUTLS_X509_FMT_PEM) <
          0 ||
      gnutls_priority_init(&ctx->priority, "NORMAL:-VERS-ALL:+VERS-TLS1.3:+VERS-TLS1.2", nullptr) <
          0) {
    tls_backend_ctx_free(ctx);
    return nullptr;
  }
  return ctx;
}

void tls_backend_ctx_free(grpc_tls_ctx *ctx) {
  if (ctx->priority != nullptr)
    gnutls_priority_deinit(ctx->priority);
  if (ctx->cred != nullptr)
    gnutls_certificate_free_credentials(ctx->cred);
  grpc_free(ctx);
}

void *tls_backend_ctx_native(grpc_tls_ctx *ctx) {
  return ctx->cred;
}

grpc_tls_session *tls_backend_session_new(grpc_tls_ctx *ctx) {
  grpc_tls_session *tls = grpc_calloc(1, sizeof(*tls));
  if (tls == nullptr) {
    return nullptr;
  }
  if (gnutls_init(&tls->session, GNUTLS_SERVER | GNUTLS_NONBLOCK) < 0) {
    grpc_free(tls);
    return nullptr;
  }

  /* clients that do not offer h2 still connect, as with the other backends */
  gnutls_datum_t alpn = {.data = (unsigned char *)"h2", .size = 2};
  if (gnutls_priority_set(tls->session, ctx->priority) < 0 ||
      gnutls_credentials_set(tls->session, GNUTLS_CRD_CERTIFICATE, ctx->cred) < 0 ||
      gnutls_alpn_set_protocols(tls->session, &alpn, 1, GNUTLS_ALPN_SERVER_PRECEDENCE) < 0) {
    tls_backend_session_free(tls);
    return nullptr;
  }
  gnutls_transport_set_ptr(tls->session, tls);
  gnutls_transport_set_pull_function(tls->session, io_pull);
  gnutls_transport_set_pull_timeout_function(tls->session, io_pull_timeout);
  gnutls_transport_set_push_function(tls->session, io_push);
  return tls;
}

void tls_backend_session_free(grpc_tls_session *tls) {
  gnutls_deinit(tls->session);
  tls_io_free(&tls->io);
  grpc_free(tls);
}

int tls_backend_feed(grpc_tls_session *tls, const void *buf, size_t len) {
  return tls_io_feed(&tls->io, buf, len);
}

int tls_backend_drain(grpc_tls_session *tls, grpc_buf *out) {
  return tls_io_drain(&tls->io, out);
}

int tls_backend_handshake(grpc_tls_session *tls) {
  int ret = gnutls_handshake(tls->session);
  if (ret == GNUTLS_E_SUCCESS) {
    return 1;
  }
  return gnutls_error_is_fatal(ret) ? -1 : 0;
}

ssize_t tls_backend_read(grpc_tls_session *tls, void *buf, size_t len) {
  for (;;) {
    ssize_t ret = gnutls_record_recv(tls->session, buf, len);
    if (ret >= 0) {
      /* 0 is the peer's close_notify */
      return ret;
    }
    if (ret == GNUTLS_E_AGAIN) {
      return -2;
    }
    if (ret == GNUTLS_E_INTERRUPTED || !gnutls_error_is_fatal((int)ret)) {
      /* e.g. a TLS 1.2 renegotiation request, which is ignored */
      continue;
    }
    return -1;
  }
}

ssize_t tls_backend_write(grpc_tls_session *tls, const void *buf, size_t len) {
  /* io_push never blocks, so gnutls_record_send() only fails on errors */
  size_t written = 0;
  while (written < len) {
    ssize_t ret = gnutls_record_send(tls->session, (const u8 *)buf + written, len - written);
    if (ret < 0) {
      if (ret == GNUTLS_E_INTERRUPTED)
        continue;
      return -1;
    }
    written += (size_t)ret;
  }
  return (ssize_t)written;
}

void tls_backend_close_notify(grpc_tls_session *tls) {
  (void)gnutls_bye(tls->session, GNUTLS_SHUT_WR);
}
