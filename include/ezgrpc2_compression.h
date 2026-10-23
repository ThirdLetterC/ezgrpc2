#ifndef EZGRPC2_COMPRESSION_H
#define EZGRPC2_COMPRESSION_H

#include "ezgrpc2_defs.h"
#include "ezgrpc2_message.h"

typedef enum {
  EZGRPC2_COMPRESS_IDENTITY = 0,
  EZGRPC2_COMPRESS_GZIP = 1,
} ezgrpc2_compression_algorithm;

/**
 * Compresses data using gzip (RFC 1952).
 * Allocates ``*out`` with ezgrpc2_malloc. Caller frees with ezgrpc2_free.
 * Returns 0 on success, non-zero on failure.
 */
EZGRPC2_API int ezgrpc2_compress_gzip(const uint8_t *in, size_t in_len, uint8_t **out,
                                      size_t *out_len);

/**
 * Decompresses gzip data (RFC 1952).
 * Allocates ``*out`` with ezgrpc2_malloc. Caller frees with ezgrpc2_free.
 * Returns 0 on success, non-zero on failure.
 */
EZGRPC2_API int ezgrpc2_decompress_gzip(const uint8_t *in, size_t in_len, uint8_t **out,
                                        size_t *out_len);

/**
 * Returns a new ezgrpc2_message with gzip-compressed payload and cflag = 1.
 * Caller frees with ezgrpc2_message_free.
 * Returns nullptr on failure.
 */
EZGRPC2_API ezgrpc2_message *ezgrpc2_message_compress_gzip(const ezgrpc2_message *msg);

/**
 * Decompresses a message (if msg->cflag == 1) using gzip and returns a new
 * ezgrpc2_message with uncompressed payload and cflag = 0.
 * If msg->cflag == 0, returns a duplicate of msg.
 * Caller frees with ezgrpc2_message_free.
 * Returns nullptr on decompression failure.
 */
EZGRPC2_API ezgrpc2_message *ezgrpc2_message_decompress_gzip(const ezgrpc2_message *msg);

#endif /* EZGRPC2_COMPRESSION_H */
