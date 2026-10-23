#include <stdlib.h>
#include <string.h>
#include "core.h"
#include "ezgrpc2_compression.h"
#include "ezgrpc2_mem.h"

#if defined(EZGRPC2_ENABLE_ZLIB)
#include <zlib.h>
#endif

EZGRPC2_API int ezgrpc2_compress_gzip(const uint8_t *in, size_t in_len, uint8_t **out,
                                      size_t *out_len) {
  if (in == nullptr || out == nullptr || out_len == nullptr) {
    return 1;
  }
#if defined(EZGRPC2_ENABLE_ZLIB)
  z_stream strm = {0};
  // 15 + 16 gives standard gzip header and trailer per zlib manual
  if (deflateInit2(&strm, Z_DEFAULT_COMPRESSION, Z_DEFLATED, 15 + 16, 8, Z_DEFAULT_STRATEGY) !=
      Z_OK) {
    return 2;
  }

  size_t buf_cap = deflateBound(&strm, (uLong)in_len) + 32;
  uint8_t *buf = ezgrpc2_malloc(buf_cap);
  if (buf == nullptr) {
    deflateEnd(&strm);
    return 3;
  }

  strm.next_in = (Bytef *)in;
  strm.avail_in = (uInt)in_len;
  strm.next_out = (Bytef *)buf;
  strm.avail_out = (uInt)buf_cap;

  int ret = deflate(&strm, Z_FINISH);
  if (ret != Z_STREAM_END) {
    ezgrpc2_free(buf);
    deflateEnd(&strm);
    return 4;
  }

  *out_len = (size_t)strm.total_out;
  *out = buf;
  deflateEnd(&strm);
  return 0;
#else
  return -1;
#endif
}

EZGRPC2_API int ezgrpc2_decompress_gzip(const uint8_t *in, size_t in_len, uint8_t **out,
                                        size_t *out_len) {
  if (in == nullptr || out == nullptr || out_len == nullptr) {
    return 1;
  }
#if defined(EZGRPC2_ENABLE_ZLIB)
  z_stream strm = {0};
  // 15 + 32 enables automatic zlib/gzip header detection
  if (inflateInit2(&strm, 15 + 32) != Z_OK) {
    return 2;
  }

  strm.next_in = (Bytef *)in;
  strm.avail_in = (uInt)in_len;

  size_t buf_cap = (in_len * 2 > 256) ? in_len * 2 : 256;
  uint8_t *buf = ezgrpc2_malloc(buf_cap);
  if (buf == nullptr) {
    inflateEnd(&strm);
    return 3;
  }

  strm.next_out = (Bytef *)buf;
  strm.avail_out = (uInt)buf_cap;

  while (1) {
    int ret = inflate(&strm, Z_NO_FLUSH);
    if (ret == Z_STREAM_END) {
      break;
    }
    if (ret != Z_OK) {
      ezgrpc2_free(buf);
      inflateEnd(&strm);
      return 4;
    }
    // Need more buffer space
    if (strm.avail_out == 0) {
      size_t old_cap = buf_cap;
      buf_cap *= 2;
      uint8_t *new_buf = ezgrpc2_realloc(buf, buf_cap);
      if (new_buf == nullptr) {
        ezgrpc2_free(buf);
        inflateEnd(&strm);
        return 3;
      }
      buf = new_buf;
      strm.next_out = (Bytef *)(buf + old_cap);
      strm.avail_out = (uInt)(buf_cap - old_cap);
    }
  }

  *out_len = (size_t)strm.total_out;
  *out = buf;
  inflateEnd(&strm);
  return 0;
#else
  return -1;
#endif
}

EZGRPC2_API ezgrpc2_message *ezgrpc2_message_compress_gzip(const ezgrpc2_message *msg) {
  if (msg == nullptr) {
    return nullptr;
  }
  uint8_t *compressed_data = nullptr;
  size_t compressed_len = 0;
  if (ezgrpc2_compress_gzip(msg->data, msg->len, &compressed_data, &compressed_len) != 0) {
    return nullptr;
  }
  ezgrpc2_message *res = message_create(1, compressed_data, compressed_len);
  ezgrpc2_free(compressed_data);
  return res;
}

EZGRPC2_API ezgrpc2_message *ezgrpc2_message_decompress_gzip(const ezgrpc2_message *msg) {
  if (msg == nullptr) {
    return nullptr;
  }
  if (msg->cflag == 0) {
    return message_create(0, msg->data, msg->len);
  }
  uint8_t *decompressed_data = nullptr;
  size_t decompressed_len = 0;
  if (ezgrpc2_decompress_gzip(msg->data, msg->len, &decompressed_data, &decompressed_len) != 0) {
    return nullptr;
  }
  ezgrpc2_message *res = message_create(0, decompressed_data, decompressed_len);
  ezgrpc2_free(decompressed_data);
  return res;
}
