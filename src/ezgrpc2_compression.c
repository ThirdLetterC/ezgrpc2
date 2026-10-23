#include <stdlib.h>
#include <string.h>
#include "core.h"
#include "ezgrpc2_compression.h"
#include "ezgrpc2_mem.h"

#if defined(EZGRPC2_ENABLE_ZLIB)
#include <zlib.h>
#endif
#if defined(EZGRPC2_ENABLE_ZSTD)
#include <zstd.h>
#endif
#if defined(EZGRPC2_ENABLE_SNAPPY)
#include <snappy-c.h>
#endif

static int zlib_codec(bool compress, bool gzip, int level, const uint8_t *in, size_t in_len,
                      uint8_t **out, size_t *out_len) {
#if defined(EZGRPC2_ENABLE_ZLIB)
  if (in_len > UINT_MAX) return 1;
  z_stream stream = {0};
  int window = gzip ? 15 + 16 : 15;
  int rc;
  if (compress) {
    if (level == EZGRPC2_COMPRESSION_LEVEL_DEFAULT) level = Z_DEFAULT_COMPRESSION;
    if (level < Z_DEFAULT_COMPRESSION || level > Z_BEST_COMPRESSION) return 1;
    rc = deflateInit2(&stream, level, Z_DEFLATED, window, 8, Z_DEFAULT_STRATEGY);
  } else {
    rc = inflateInit2(&stream, window);
  }
  if (rc != Z_OK) return 2;
  size_t cap = compress ? (size_t)deflateBound(&stream, (uLong)in_len) : (in_len < 256 ? 256 : in_len * 2);
  if (cap < in_len || cap > UINT_MAX) { (compress ? deflateEnd : inflateEnd)(&stream); return 1; }
  uint8_t *buf = ezgrpc2_malloc(cap ? cap : 1);
  if (!buf) { (compress ? deflateEnd : inflateEnd)(&stream); return 3; }
  stream.next_in = (Bytef *)in;
  stream.avail_in = (uInt)in_len;
  int status = Z_OK;
  do {
    stream.next_out = buf + stream.total_out;
    stream.avail_out = (uInt)(cap - stream.total_out);
    status = compress ? deflate(&stream, Z_FINISH) : inflate(&stream, Z_NO_FLUSH);
    if ((compress && status == Z_STREAM_END) || (!compress && status == Z_STREAM_END)) break;
    if (status != Z_OK || stream.total_out == cap) {
      size_t next_cap = cap > SIZE_MAX / 2 ? 0 : cap * 2;
      if (!next_cap || next_cap > UINT_MAX) { ezgrpc2_free(buf); (compress ? deflateEnd : inflateEnd)(&stream); return 4; }
      uint8_t *next = ezgrpc2_realloc(buf, next_cap);
      if (!next) { ezgrpc2_free(buf); (compress ? deflateEnd : inflateEnd)(&stream); return 3; }
      buf = next;
      cap = next_cap;
    }
  } while (true);
  *out_len = (size_t)stream.total_out;
  *out = buf;
  (compress ? deflateEnd : inflateEnd)(&stream);
  return 0;
#else
  (void)compress; (void)gzip; (void)level; (void)in; (void)in_len; (void)out; (void)out_len;
  return -1;
#endif
}

EZGRPC2_API int ezgrpc2_compress(ezgrpc2_compression_algorithm algorithm, int level,
                                 const uint8_t *in, size_t in_len, uint8_t **out,
                                 size_t *out_len) {
  if ((!in && in_len) || !out || !out_len) return 1;
  static const uint8_t empty_input = 0;
  if (!in) in = &empty_input;
  *out = nullptr; *out_len = 0;
  switch (algorithm) {
  case EZGRPC2_COMPRESS_GZIP:
  case EZGRPC2_COMPRESS_DEFLATE:
    return zlib_codec(true, algorithm == EZGRPC2_COMPRESS_GZIP, level, in, in_len, out, out_len);
  case EZGRPC2_COMPRESS_ZSTD:
#if defined(EZGRPC2_ENABLE_ZSTD)
  {
    if (level == EZGRPC2_COMPRESSION_LEVEL_DEFAULT) level = ZSTD_CLEVEL_DEFAULT;
    if (level < ZSTD_minCLevel() || level > ZSTD_maxCLevel()) return 1;
    size_t cap = ZSTD_compressBound(in_len);
    uint8_t *buf = ezgrpc2_malloc(cap ? cap : 1);
    if (!buf) return 3;
    size_t n = ZSTD_compress(buf, cap, in, in_len, level);
    if (ZSTD_isError(n)) { ezgrpc2_free(buf); return 4; }
    *out = buf; *out_len = n; return 0;
  }
#else
    return -1;
#endif
  case EZGRPC2_COMPRESS_SNAPPY:
#if defined(EZGRPC2_ENABLE_SNAPPY)
  {
    if (level != EZGRPC2_COMPRESSION_LEVEL_DEFAULT) return 1;
    size_t cap = snappy_max_compressed_length(in_len);
    uint8_t *buf = ezgrpc2_malloc(cap ? cap : 1);
    if (!buf) return 3;
    size_t n = cap;
    if (snappy_compress((const char *)in, in_len, (char *)buf, &n) != SNAPPY_OK) { ezgrpc2_free(buf); return 4; }
    *out = buf; *out_len = n; return 0;
  }
#else
    return -1;
#endif
  default: return -1;
  }
}

EZGRPC2_API int ezgrpc2_decompress(ezgrpc2_compression_algorithm algorithm, const uint8_t *in,
                                   size_t in_len, uint8_t **out, size_t *out_len) {
  if ((!in && in_len) || !out || !out_len) return 1;
  static const uint8_t empty_input = 0;
  if (!in) in = &empty_input;
  *out = nullptr; *out_len = 0;
  if (algorithm == EZGRPC2_COMPRESS_GZIP || algorithm == EZGRPC2_COMPRESS_DEFLATE)
    return zlib_codec(false, algorithm == EZGRPC2_COMPRESS_GZIP, 0, in, in_len, out, out_len);
  if (algorithm == EZGRPC2_COMPRESS_ZSTD) {
#if defined(EZGRPC2_ENABLE_ZSTD)
    ZSTD_DCtx *ctx = ZSTD_createDCtx();
    if (!ctx) return 3;
    size_t cap = 4096, used = 0;
    uint8_t *buf = ezgrpc2_malloc(cap);
    if (!buf) { ZSTD_freeDCtx(ctx); return 3; }
    ZSTD_inBuffer input = {in, in_len, 0};
    size_t remaining = 1;
    while (remaining != 0 || input.pos < input.size) {
      if (used == cap) {
        if (cap > SIZE_MAX / 2) { ezgrpc2_free(buf); ZSTD_freeDCtx(ctx); return 4; }
        size_t next_cap = cap * 2;
        uint8_t *next = ezgrpc2_realloc(buf, next_cap);
        if (!next) { ezgrpc2_free(buf); ZSTD_freeDCtx(ctx); return 3; }
        buf = next; cap = next_cap;
      }
      ZSTD_outBuffer output = {buf + used, cap - used, 0};
      size_t before = input.pos;
      remaining = ZSTD_decompressStream(ctx, &output, &input);
      if (ZSTD_isError(remaining) || (before == input.pos && output.pos == 0)) {
        ezgrpc2_free(buf); ZSTD_freeDCtx(ctx); return 4;
      }
      used += output.pos;
      if (input.pos == input.size && remaining != 0) {
        ezgrpc2_free(buf); ZSTD_freeDCtx(ctx); return 4;
      }
    }
    ZSTD_freeDCtx(ctx);
    *out = buf; *out_len = used; return 0;
#else
    return -1;
#endif
  }
  if (algorithm == EZGRPC2_COMPRESS_SNAPPY) {
#if defined(EZGRPC2_ENABLE_SNAPPY)
    size_t cap;
    if (snappy_uncompressed_length((const char *)in, in_len, &cap) != SNAPPY_OK) return 4;
    uint8_t *buf = ezgrpc2_malloc(cap ? cap : 1);
    if (!buf) return 3;
    if (snappy_uncompress((const char *)in, in_len, (char *)buf, &cap) != SNAPPY_OK) { ezgrpc2_free(buf); return 4; }
    *out = buf; *out_len = cap; return 0;
#else
    return -1;
#endif
  }
  return -1;
}

EZGRPC2_API int ezgrpc2_compress_gzip(const uint8_t *in, size_t len, uint8_t **out, size_t *out_len) {
  return ezgrpc2_compress(EZGRPC2_COMPRESS_GZIP, EZGRPC2_COMPRESSION_LEVEL_DEFAULT, in, len, out, out_len);
}
EZGRPC2_API int ezgrpc2_decompress_gzip(const uint8_t *in, size_t len, uint8_t **out, size_t *out_len) {
  return ezgrpc2_decompress(EZGRPC2_COMPRESS_GZIP, in, len, out, out_len);
}

EZGRPC2_API ezgrpc2_message *ezgrpc2_message_compress(const ezgrpc2_message *msg,
                                                       ezgrpc2_compression_algorithm algorithm, int level) {
  if (!msg) return nullptr;
  uint8_t *data = nullptr; size_t len = 0;
  if (ezgrpc2_compress(algorithm, level, msg->data, msg->len, &data, &len) != 0) return nullptr;
  ezgrpc2_message *result = message_create(1, data, len);
  ezgrpc2_free(data);
  if (result) { result->compression = algorithm; result->compression_level = level; }
  return result;
}
EZGRPC2_API ezgrpc2_message *ezgrpc2_message_decompress(const ezgrpc2_message *msg,
                                                        ezgrpc2_compression_algorithm algorithm) {
  if (!msg) return nullptr;
  if (!msg->cflag) return message_create(0, msg->data, msg->len);
  uint8_t *data = nullptr; size_t len = 0;
  if (ezgrpc2_decompress(algorithm, msg->data, msg->len, &data, &len) != 0) return nullptr;
  ezgrpc2_message *result = message_create(0, data, len);
  ezgrpc2_free(data);
  return result;
}
EZGRPC2_API ezgrpc2_message *ezgrpc2_message_compress_gzip(const ezgrpc2_message *msg) {
  return ezgrpc2_message_compress(msg, EZGRPC2_COMPRESS_GZIP, EZGRPC2_COMPRESSION_LEVEL_DEFAULT);
}
EZGRPC2_API ezgrpc2_message *ezgrpc2_message_decompress_gzip(const ezgrpc2_message *msg) {
  return ezgrpc2_message_decompress(msg, EZGRPC2_COMPRESS_GZIP);
}
