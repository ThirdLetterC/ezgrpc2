#ifndef EZGRPC2_TLS_H
#define EZGRPC2_TLS_H

#include "ezgrpc2_defs.h"


/**
 * TLS is enabled with ezgrpc2_config.tls. The library is built with one of
 * OpenSSL, LibreSSL, BoringSSL, AWS-LC, wolfSSL, GnuTLS or rustls
 * (``-DEZGRPC2_TLS_BACKEND=...`` in CMake). For settings ezgrpc2 does not expose,
 * ezgrpc2_server_tls_native() returns the library's own context:
 *
 *   * OpenSSL, LibreSSL, BoringSSL, AWS-LC: ``SSL_CTX *``
 *   * wolfSSL: ``WOLFSSL_CTX *``
 *   * GnuTLS: ``gnutls_certificate_credentials_t``
 *   * rustls: ``const struct rustls_server_config *``
 */

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

#endif /* EZGRPC2_TLS_H */
