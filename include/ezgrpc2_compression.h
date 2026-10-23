#pragma once

#include "ezgrpc2_defs.h"
#include "ezgrpc2_message.h"

#define EZGRPC2_COMPRESSION_LEVEL_DEFAULT (-1)

/** Compresses using the selected codec and level; output is freed with ezgrpc2_free(). */
EZGRPC2_API int ezgrpc2_compress(ezgrpc2_compression_algorithm algorithm, int level,
                                 const uint8_t *in, size_t in_len, uint8_t **out,
                                 size_t *out_len);
/** Decompresses using the selected codec; output is freed with ezgrpc2_free(). */
EZGRPC2_API int ezgrpc2_decompress(ezgrpc2_compression_algorithm algorithm,
                                   const uint8_t *in, size_t in_len, uint8_t **out,
                                   size_t *out_len);
/** Returns a compressed copy of msg using algorithm and level. */
EZGRPC2_API ezgrpc2_message *ezgrpc2_message_compress(const ezgrpc2_message *msg,
                                                       ezgrpc2_compression_algorithm algorithm,
                                                       int level);
/** Decompresses msg using algorithm; uncompressed messages are duplicated. */
EZGRPC2_API ezgrpc2_message *ezgrpc2_message_decompress(
    const ezgrpc2_message *msg, ezgrpc2_compression_algorithm algorithm);

/**
 * Compresses data using gzip (RFC 1952).
 * Allocates ``*out`` with ezgrpc2_malloc(). The caller frees it with ezgrpc2_free().
 * Returns 0 on success, non-zero on failure.
 */
EZGRPC2_API int ezgrpc2_compress_gzip(const uint8_t *in, size_t in_len, uint8_t **out,
                                      size_t *out_len);

/**
 * Decompresses gzip data (RFC 1952).
 * Allocates ``*out`` with ezgrpc2_malloc(). The caller frees it with ezgrpc2_free().
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
