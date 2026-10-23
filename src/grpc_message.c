#include <string.h>
#include "core.h"
#include "grpc_message.h"

/* messages start on a cache line */
enum { MESSAGE_ALIGN = 64 };
static_assert((MESSAGE_ALIGN & (MESSAGE_ALIGN - 1)) == 0, "MESSAGE_ALIGN must be a power of two");

grpc_message *message_create(u8 cflag, const void *data, size_t len) {
  if (len > UINT32_MAX)
    return nullptr;
  size_t size = sizeof(grpc_message) + len;
  grpc_message *message = grpc_aligned_alloc(MESSAGE_ALIGN,
                                                   grpc_align_up(size, MESSAGE_ALIGN));
  if (message == nullptr)
    return nullptr;
  message->len = (uint32_t)len;
  message->cflag = cflag;
  message->compression = GRPC_COMPRESS_IDENTITY;
  message->compression_level = -1;
  if (data != nullptr && len > 0)
    memcpy(message->data, data, len);
  return message;
}

GRPC_API grpc_message *grpc_message_new(const void *data, size_t len) {
  if (data == nullptr && len > 0)
    return nullptr;
  return message_create(0, data, len);
}

GRPC_API grpc_message *grpc_message_alloc(size_t len) {
  return message_create(0, nullptr, len);
}

GRPC_API void grpc_message_free(grpc_message *message) {
  grpc_free(message);
}
