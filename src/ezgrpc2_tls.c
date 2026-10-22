#include <stdlib.h>
#include <string.h>
#include <stdio.h>
#include "ezgrpc2_tls.h"
#include "ezgrpc2_server.h"
#include "core.h"

#if defined(EZGRPC2_ENABLE_OPENSSL)
#include <openssl/ssl.h>
#include <openssl/err.h>

static int alpn_select_cb(
  [[maybe_unused]] SSL *ssl,
  const unsigned char **out,
  unsigned char *outlen,
  const unsigned char *in,
  unsigned int inlen,
  [[maybe_unused]] void *arg) {
  // HTTP/2 ALPN token is "\x02h2"
  static const unsigned char h2_proto[] = "\x02h2";
  if (SSL_select_next_proto((unsigned char **)out, outlen,
                            h2_proto, sizeof(h2_proto) - 1, in, inlen) != OPENSSL_NPN_NEGOTIATED) {
    return SSL_TLSEXT_ERR_NOACK;
  }
  return SSL_TLSEXT_ERR_OK;
}

EZGRPC2_API SSL_CTX *ezgrpc2_tls_init_server_ctx(const char *cert_file, const char *key_file) {
  if (cert_file == nullptr || key_file == nullptr) {
    return nullptr;
  }

  const SSL_METHOD *method = TLS_server_method();
  SSL_CTX *ctx = SSL_CTX_new(method);
  if (ctx == nullptr) {
    return nullptr;
  }

  // Require modern TLS 1.2+
  SSL_CTX_set_min_proto_version(ctx, TLS1_2_VERSION);
  SSL_CTX_set_options(ctx, SSL_OP_NO_SSLv2 | SSL_OP_NO_SSLv3 | SSL_OP_NO_TLSv1 | SSL_OP_NO_TLSv1_1);

  // Set ALPN negotiation for HTTP/2 ("h2")
  SSL_CTX_set_alpn_select_cb(ctx, alpn_select_cb, nullptr);

  // Load certificate chain
  if (SSL_CTX_use_certificate_chain_file(ctx, cert_file) <= 0) {
    SSL_CTX_free(ctx);
    return nullptr;
  }

  // Load private key
  if (SSL_CTX_use_PrivateKey_file(ctx, key_file, SSL_FILETYPE_PEM) <= 0) {
    SSL_CTX_free(ctx);
    return nullptr;
  }

  // Verify private key matches certificate
  if (!SSL_CTX_check_private_key(ctx)) {
    SSL_CTX_free(ctx);
    return nullptr;
  }

  return ctx;
}

EZGRPC2_API void ezgrpc2_tls_free_server_ctx(SSL_CTX *ctx) {
  if (ctx != nullptr) {
    SSL_CTX_free(ctx);
  }
}

EZGRPC2_API int ezgrpc2_server_enable_tls(
  ezgrpc2_server *server,
  const char *cert_file,
  const char *key_file) {
  if (server == nullptr || cert_file == nullptr || key_file == nullptr) {
    return 1;
  }

  SSL_CTX *ctx = ezgrpc2_tls_init_server_ctx(cert_file, key_file);
  if (ctx == nullptr) {
    return 2;
  }

  if (server->ssl_ctx != nullptr) {
    SSL_CTX_free(server->ssl_ctx);
  }

  server->ssl_ctx = ctx;
  server->is_tls = 1;
  return 0;
}

int ezgrpc2_tls_session_init(ezgrpc2_session *session) {
  if (session == nullptr || session->server == nullptr || !session->server->is_tls || session->server->ssl_ctx == nullptr) {
    return 0;
  }

  SSL *ssl = SSL_new(session->server->ssl_ctx);
  if (ssl == nullptr) {
    return 1;
  }

  SSL_set_fd(ssl, session->sockfd);
  SSL_set_accept_state(ssl);

  session->ssl = ssl;
  session->is_tls = 1;
  session->tls_handshake_done = 0;
  return 0;
}

int ezgrpc2_tls_handshake(ezgrpc2_session *session) {
  if (session == nullptr || !session->is_tls || session->ssl == nullptr) {
    return 1;
  }
  if (session->tls_handshake_done) {
    return 1;
  }

  int ret = SSL_accept(session->ssl);
  if (ret == 1) {
    session->tls_handshake_done = 1;
    return 1;
  }

  int err = SSL_get_error(session->ssl, ret);
  if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
    return 0;
  }

  return -1;
}

ssize_t ezgrpc2_tls_read(ezgrpc2_session *session, void *buf, size_t len) {
  if (session == nullptr || !session->is_tls || session->ssl == nullptr) {
    return -1;
  }

  int ret = SSL_read(session->ssl, buf, (int)len);
  if (ret > 0) {
    return (ssize_t)ret;
  }

  int err = SSL_get_error(session->ssl, ret);
  if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
    return -2;
  }
  if (err == SSL_ERROR_ZERO_RETURN) {
    return 0;
  }

  return -1;
}

ssize_t ezgrpc2_tls_write(ezgrpc2_session *session, const void *buf, size_t len) {
  if (session == nullptr || !session->is_tls || session->ssl == nullptr) {
    return -1;
  }

  int ret = SSL_write(session->ssl, buf, (int)len);
  if (ret > 0) {
    return (ssize_t)ret;
  }

  int err = SSL_get_error(session->ssl, ret);
  if (err == SSL_ERROR_WANT_READ || err == SSL_ERROR_WANT_WRITE) {
    return -2;
  }
  if (err == SSL_ERROR_ZERO_RETURN) {
    return 0;
  }

  return -1;
}

void ezgrpc2_tls_session_free(ezgrpc2_session *session) {
  if (session != nullptr && session->ssl != nullptr) {
    SSL_shutdown(session->ssl);
    SSL_free(session->ssl);
    session->ssl = nullptr;
    session->is_tls = 0;
    session->tls_handshake_done = 0;
  }
}

#else

EZGRPC2_API SSL_CTX *ezgrpc2_tls_init_server_ctx([[maybe_unused]] const char *cert_file, [[maybe_unused]] const char *key_file) {
  return nullptr;
}

EZGRPC2_API void ezgrpc2_tls_free_server_ctx([[maybe_unused]] SSL_CTX *ctx) {}

EZGRPC2_API int ezgrpc2_server_enable_tls([[maybe_unused]] ezgrpc2_server *server, [[maybe_unused]] const char *cert_file, [[maybe_unused]] const char *key_file) {
  return -1;
}

int ezgrpc2_tls_session_init([[maybe_unused]] ezgrpc2_session *session) {
  return 0;
}

int ezgrpc2_tls_handshake([[maybe_unused]] ezgrpc2_session *session) {
  return 1;
}

ssize_t ezgrpc2_tls_read([[maybe_unused]] ezgrpc2_session *session, [[maybe_unused]] void *buf, [[maybe_unused]] size_t len) {
  return -1;
}

ssize_t ezgrpc2_tls_write([[maybe_unused]] ezgrpc2_session *session, [[maybe_unused]] const void *buf, [[maybe_unused]] size_t len) {
  return -1;
}

void ezgrpc2_tls_session_free([[maybe_unused]] ezgrpc2_session *session) {}

#endif

EZGRPC2_API ezgrpc2_server *ezgrpc2_server_new_tls(
  const char *ipv4_addr,
  uint16_t ipv4_port,
  const char *ipv6_addr,
  uint16_t ipv6_port,
  int backlog,
  const char *cert_file,
  const char *key_file,
  ezgrpc2_server_settings *server_settings,
  ezgrpc2_http2_settings *http2_settings) {
  ezgrpc2_server *server = ezgrpc2_server_new(
    ipv4_addr, ipv4_port, ipv6_addr, ipv6_port, backlog,
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
