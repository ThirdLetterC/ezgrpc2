#include <string.h>
#include "ezgrpc2_flatcc.h"
#include "ezgrpc2_mem.h"

/* flatcc_builder_default_alloc(), on the ezgrpc2 allocator */
static int fb_alloc([[maybe_unused]] void *alloc_context, flatcc_iovec_t *b, size_t request,
                    int zero_fill, int alloc_type) {
  if (request == 0) {
    ezgrpc2_free(b->iov_base);
    b->iov_base = nullptr;
    b->iov_len = 0;
    return 0;
  }

  size_t n;
  switch (alloc_type) {
  case flatcc_builder_alloc_ds:
    n = 256;
    break;
  case flatcc_builder_alloc_ht:
    /* the hash table uses exactly what it asks for */
    n = request;
    break;
  case flatcc_builder_alloc_fs:
    n = sizeof(__flatcc_builder_frame_t) * 8;
    break;
  case flatcc_builder_alloc_us:
    n = 64;
    break;
  default:
    n = 32;
    break;
  }
  while (n < request) {
    n *= 2;
  }
  /* as flatcc does: a buffer twice the needed size or more is kept, not shrunk */
  if (request <= b->iov_len && b->iov_len / 2 >= n) {
    return 0;
  }

  void *p = ezgrpc2_realloc(b->iov_base, n);
  if (p == nullptr) {
    return -1;
  }
  if (zero_fill && b->iov_len < n) {
    memset((uint8_t *)p + b->iov_len, 0, n - b->iov_len);
  }
  b->iov_base = p;
  b->iov_len = n;
  return 0;
}

EZGRPC2_API ezgrpc2_err ezgrpc2_fb_builder_init(flatcc_builder_t *builder) {
  if (builder == nullptr) {
    return EZGRPC2_ERR_INVALID;
  }
  return flatcc_builder_custom_init(builder, nullptr, nullptr, fb_alloc, nullptr) == 0
             ? EZGRPC2_OK
             : EZGRPC2_ERR_NOMEM;
}

EZGRPC2_API int ezgrpc2_fb_verify(const ezgrpc2_message *message, const char *fid,
                                  flatcc_table_verifier_f *verifier) {
  if (message == nullptr || verifier == nullptr) {
    return flatcc_verify_error_buffer_header_too_small;
  }
  return flatcc_verify_table_as_root(message->data, message->len, fid, verifier);
}

EZGRPC2_API const void *ezgrpc2_fb_unpack(const ezgrpc2_message *message, const char *fid,
                                          flatcc_table_verifier_f *verifier) {
  if (ezgrpc2_fb_verify(message, fid, verifier) != flatcc_verify_ok) {
    return nullptr;
  }
  /* a buffer starts with the little endian offset of its root table */
  const uint8_t *p = message->data;
  uint32_t root =
      (uint32_t)p[0] | (uint32_t)p[1] << 8 | (uint32_t)p[2] << 16 | (uint32_t)p[3] << 24;
  return p + root;
}

EZGRPC2_API ezgrpc2_message *ezgrpc2_fb_pack(flatcc_builder_t *builder) {
  if (builder == nullptr) {
    return nullptr;
  }

  size_t size = flatcc_builder_get_buffer_size(builder);
  if (size == 0 || size > UINT32_MAX) {
    return nullptr;
  }

  ezgrpc2_message *msg = ezgrpc2_message_alloc(size);
  if (msg == nullptr) {
    return nullptr;
  }
  if (flatcc_builder_copy_buffer(builder, msg->data, size) == nullptr) {
    ezgrpc2_message_free(msg);
    return nullptr;
  }

  flatcc_builder_reset(builder);
  return msg;
}

EZGRPC2_API ezgrpc2_err ezgrpc2_call_send_fb(ezgrpc2_call *call, flatcc_builder_t *builder) {
  if (call == nullptr || builder == nullptr) {
    return EZGRPC2_ERR_INVALID;
  }
  ezgrpc2_message *msg = ezgrpc2_fb_pack(builder);
  if (msg == nullptr) {
    return flatcc_builder_get_buffer_size(builder) == 0 ? EZGRPC2_ERR_INVALID : EZGRPC2_ERR_NOMEM;
  }
  return ezgrpc2_call_send(call, msg);
}
