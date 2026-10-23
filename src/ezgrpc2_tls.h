#ifndef EZGRPC2_TLS_H
#define EZGRPC2_TLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

struct ezgrpc2_server;
struct ezgrpc2_session;
struct ezgrpc2_buf;

#if defined(EZGRPC2_ENABLE_OPENSSL)
typedef struct ssl_ctx_st SSL_CTX;
typedef struct ssl_st SSL;
#else
typedef void SSL_CTX;
typedef void SSL;
#endif

/**
 * Initializes and configures an OpenSSL SSL_CTX for HTTP/2 ALPN ("h2").
 * Loads certificate and private key files.
 * Returns SSL_CTX on success, nullptr on error.
 */
EZGRPC2_API SSL_CTX *ezgrpc2_tls_init_server_ctx(const char *cert_file, const char *key_file);

/**
 * Frees an SSL_CTX.
 */
EZGRPC2_API void ezgrpc2_tls_free_server_ctx(SSL_CTX *ctx);

/**
 * Enables TLS on an existing ezgrpc2_server.
 * Returns 0 on success, non-zero on error.
 */
EZGRPC2_API int ezgrpc2_server_enable_tls(
  struct ezgrpc2_server *server,
  const char *cert_file,
  const char *key_file
);

/**
 * Creates an ezgrpc2_server configured for TLS over HTTP/2.
 */
struct ezgrpc2_http2_settings;
struct ezgrpc2_server_settings;

EZGRPC2_API struct ezgrpc2_server *ezgrpc2_server_new_tls(
  const char *ipv4_addr,
  uint16_t ipv4_port,
  const char *ipv6_addr,
  uint16_t ipv6_port,
  int backlog,
  const char *cert_file,
  const char *key_file,
  struct ezgrpc2_server_settings *server_settings,
  struct ezgrpc2_http2_settings *http2_settings
);

/**
 * Initializes SSL object for a newly accepted session if server has TLS enabled.
 * The SSL object never touches the socket: ciphertext is exchanged through
 * memory BIOs with :c:func:`ezgrpc2_tls_feed()` and :c:func:`ezgrpc2_tls_drain()`
 * so that the event loop owns all socket I/O.
 * Returns 0 on success, non-zero on error.
 */
int ezgrpc2_tls_session_init(struct ezgrpc2_session *session);

/**
 * Hands ciphertext received from the socket to the TLS layer.
 * Returns 0 on success, non-zero on error.
 */
int ezgrpc2_tls_feed(struct ezgrpc2_session *session, const void *buf, size_t len);

/**
 * Appends the ciphertext produced by the TLS layer (handshake records and
 * encrypted application data) to ``out``.
 * Returns 0 on success, non-zero on error.
 */
int ezgrpc2_tls_drain(struct ezgrpc2_session *session, struct ezgrpc2_buf *out);

/**
 * Performs non-blocking TLS handshake for a session.
 * Returns:
 *   1  if handshake is complete
 *   0  if handshake wants read/write (WANT_READ or WANT_WRITE)
 *  -1  on fatal handshake failure
 */
int ezgrpc2_tls_handshake(struct ezgrpc2_session *session);

/**
 * Reads decrypted bytes from session TLS layer.
 * Returns bytes read, or negative error code:
 *   -2 for WANT_READ / WANT_WRITE
 *   -1 for error / close
 */
ssize_t ezgrpc2_tls_read(struct ezgrpc2_session *session, void *buf, size_t len);

/**
 * Writes plaintext bytes into session TLS layer.
 * Returns bytes written, or negative error code:
 *   -2 for WANT_READ / WANT_WRITE
 *   -1 for error / close
 */
ssize_t ezgrpc2_tls_write(struct ezgrpc2_session *session, const void *buf, size_t len);

/**
 * Queues a close_notify alert, to be collected with :c:func:`ezgrpc2_tls_drain()`.
 */
void ezgrpc2_tls_close_notify(struct ezgrpc2_session *session);

/**
 * Frees session SSL object.
 */
void ezgrpc2_tls_session_free(struct ezgrpc2_session *session);

#ifdef __cplusplus
}
#endif

#endif /* EZGRPC2_TLS_H */
