#pragma once

#include "grpc_defs.h"

/**
 * A metadata entry (an HTTP/2 header): sent with
 * :c:func:`grpc_call_send_metadata()` and :c:func:`grpc_call_add_trailers()`,
 * and returned by :c:func:`grpc_call_metadata_at()`.
 *
 * When sending, a length of 0 means ``strlen()``. Keys must be lowercase.
 * Keys ending in ``-bin`` carry Base64 encoded binary values.
 */
typedef struct grpc_metadata {
  const char *key;
  size_t key_len;
  const char *value;
  size_t value_len;
} grpc_metadata;

/**
 * Decodes the Base64 value of a ``-bin`` key.
 * Returns a buffer to free with grpc_free(), or nullptr on invalid input.
 */
GRPC_API uint8_t *grpc_metadata_decode_bin(const char *base64, size_t len, size_t *out_len);

/**
 * Base64 encodes binary data for a ``-bin`` key.
 * Returns a NUL-terminated string to free with grpc_free(), or nullptr.
 */
GRPC_API char *grpc_metadata_encode_bin(const uint8_t *data, size_t len, size_t *out_len);
