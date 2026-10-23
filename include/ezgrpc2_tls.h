#ifndef EZGRPC2_TLS_H
#define EZGRPC2_TLS_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include <unistd.h>
#include "defs.h"

struct ezgrpc2_server;
struct ezgrpc2_session;
struct ezgrpc2_buf;
struct ezgrpc2_http2_settings;
struct ezgrpc2_server_settings;

/**
 * An opaque TLS server context: a certificate chain, its private key, TLS 1.2+
 * and ALPN ``h2``. It is backed by the TLS library selected at build time
 * (``make TLS=...``): OpenSSL, LibreSSL, BoringSSL, AWS-LC, wolfSSL, GnuTLS
 * or rustls.
 */
typedef struct ezgrpc2_tls_ctx ezgrpc2_tls_ctx;

/**
 * Creates a TLS server context from PEM encoded certificate chain and private
 * key files.
 * Returns nullptr on error, or when the library is built without TLS.
 */
EZGRPC2_API ezgrpc2_tls_ctx *ezgrpc2_tls_init_server_ctx(const char *cert_file,
                                                         const char *key_file);

/**
 * Frees a context created by :c:func:`ezgrpc2_tls_init_server_ctx()`.
 */
EZGRPC2_API void ezgrpc2_tls_free_server_ctx(ezgrpc2_tls_ctx *ctx);

/**
 * Returns the TLS library's own context, for settings ezgrpc2 does not expose:
 *
 *   * OpenSSL, LibreSSL, BoringSSL, AWS-LC: ``SSL_CTX *``
 *   * wolfSSL: ``WOLFSSL_CTX *``
 *   * GnuTLS: ``gnutls_certificate_credentials_t``
 *   * rustls: ``const struct rustls_server_config *``
 *
 * Returns nullptr when the library is built without TLS.
 */
EZGRPC2_API void *ezgrpc2_tls_ctx_native(ezgrpc2_tls_ctx *ctx);

/**
 * Returns the name of the TLS backend the library was built with: ``"openssl"``,
 * ``"libressl"``, ``"boringssl"``, ``"awslc"``, ``"wolfssl"``, ``"gnutls"``,
 * ``"rustls"``, or ``"none"``.
 */
EZGRPC2_API const char *ezgrpc2_tls_backend(void);

/**
 * Returns the version string reported by the TLS library in use, or ``""``.
 */
EZGRPC2_API const char *ezgrpc2_tls_backend_version(void);

/**
 * Enables TLS on an existing ezgrpc2_server.
 * Returns 0 on success, non-zero on error.
 */
EZGRPC2_API int ezgrpc2_server_enable_tls(struct ezgrpc2_server *server, const char *cert_file,
                                          const char *key_file);

/**
 * Creates an ezgrpc2_server configured for TLS over HTTP/2.
 */
EZGRPC2_API struct ezgrpc2_server *
ezgrpc2_server_new_tls(const char *ipv4_addr, uint16_t ipv4_port, const char *ipv6_addr,
                       uint16_t ipv6_port, int backlog, const char *cert_file, const char *key_file,
                       struct ezgrpc2_server_settings *server_settings,
                       struct ezgrpc2_http2_settings *http2_settings);

#endif /* EZGRPC2_TLS_H */
