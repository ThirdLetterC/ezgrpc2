#pragma once

#include "ezgrpc2_defs.h"

#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#include <stdalign.h>
#endif

typedef struct ezgrpc2_message ezgrpc2_message;

typedef enum {
  EZGRPC2_COMPRESS_IDENTITY = 0,
  EZGRPC2_COMPRESS_GZIP = 1,
  EZGRPC2_COMPRESS_DEFLATE = 2,
  EZGRPC2_COMPRESS_ZSTD = 3,
  EZGRPC2_COMPRESS_SNAPPY = 4,
} ezgrpc2_compression_algorithm;

/**
 * A gRPC message: one serialized request or response.
 */
struct ezgrpc2_message {
  /** Length of ``data`` in bytes. */
  uint32_t len;
  /** 1 if ``data`` is compressed (the gRPC Compressed-Flag), otherwise 0. */
  uint8_t cflag;
  /** Encoding used when cflag is 1; ignored for uncompressed messages. */
  ezgrpc2_compression_algorithm compression;
  /** Codec level, or EZGRPC2_COMPRESSION_LEVEL_DEFAULT. */
  int compression_level;
  /** The payload, aligned like ``malloc()`` memory, so formats that are read
   * in place (FlatBuffers) can use it directly. */
  alignas(max_align_t) uint8_t data[];
};

/**
 * Creates a message holding a copy of ``data``.
 * Returns nullptr when out of memory.
 */
EZGRPC2_API ezgrpc2_message *ezgrpc2_message_new(const void *data, size_t len);

/**
 * Creates a message with ``len`` uninitialized bytes, for serializing in place.
 * Returns nullptr when out of memory.
 */
EZGRPC2_API ezgrpc2_message *ezgrpc2_message_alloc(size_t len);

/**
 * Frees a message. Accepts nullptr.
 */
EZGRPC2_API void ezgrpc2_message_free(ezgrpc2_message *message);
