#pragma once

#include "grpc_defs.h"


/**
 * TLS is enabled with grpc_config.tls. The library is built with one of
 * OpenSSL, LibreSSL, BoringSSL, AWS-LC, wolfSSL, GnuTLS or rustls
 * (``-DGRPC_TLS_BACKEND=...`` in CMake). For settings grpc does not expose,
 * grpc_server_tls_native() returns the library's own context:
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
GRPC_API const char *grpc_tls_backend(void);

/**
 * Returns the version string reported by the TLS library in use, or ``""``.
 */
GRPC_API const char *grpc_tls_backend_version(void);
