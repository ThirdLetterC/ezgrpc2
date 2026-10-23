#ifndef EZGRPC2_METADATA_H
#define EZGRPC2_METADATA_H

#include "ezgrpc2_defs.h"

/**
 * A metadata entry (an HTTP/2 header): sent with
 * :c:func:`ezgrpc2_call_send_metadata()` and :c:func:`ezgrpc2_call_add_trailers()`,
 * and returned by :c:func:`ezgrpc2_call_metadata_at()`.
 *
 * When sending, a length of 0 means ``strlen()``. Keys must be lowercase.
 * Keys ending in ``-bin`` carry Base64 encoded binary values.
 */
typedef struct ezgrpc2_metadata {
  const char *key;
  size_t key_len;
  const char *value;
  size_t value_len;
} ezgrpc2_metadata;

/**
 * Decodes the Base64 value of a ``-bin`` key.
 * Returns a buffer to free with ezgrpc2_free(), or nullptr on invalid input.
 */
EZGRPC2_API uint8_t *ezgrpc2_metadata_decode_bin(const char *base64, size_t len, size_t *out_len);

/**
 * Base64 encodes binary data for a ``-bin`` key.
 * Returns a NULL-terminated string to free with ezgrpc2_free(), or nullptr.
 */
EZGRPC2_API char *ezgrpc2_metadata_encode_bin(const uint8_t *data, size_t len, size_t *out_len);

#endif /* EZGRPC2_METADATA_H */
