#include <string.h>
#include "core.h"
#include "ezgrpc2_message.h"

/* messages start on a cache line */
static constexpr size_t MESSAGE_ALIGN = 64;
static_assert((MESSAGE_ALIGN & (MESSAGE_ALIGN - 1)) == 0, "MESSAGE_ALIGN must be a power of two");

ezgrpc2_message *message_create(u8 cflag, const void *data, size_t len) {
  if (len > UINT32_MAX)
    return nullptr;
  size_t size = sizeof(ezgrpc2_message) + len;
  ezgrpc2_message *message = ezgrpc2_aligned_alloc(MESSAGE_ALIGN,
                                                   ezgrpc2_align_up(size, MESSAGE_ALIGN));
  if (message == nullptr)
    return nullptr;
  message->len = (uint32_t)len;
  message->cflag = cflag;
  if (data != nullptr && len > 0)
    memcpy(message->data, data, len);
  return message;
}

EZGRPC2_API ezgrpc2_message *ezgrpc2_message_new(const void *data, size_t len) {
  if (data == nullptr && len > 0)
    return nullptr;
  return message_create(0, data, len);
}

EZGRPC2_API ezgrpc2_message *ezgrpc2_message_alloc(size_t len) {
  return message_create(0, nullptr, len);
}

EZGRPC2_API void ezgrpc2_message_free(ezgrpc2_message *message) {
  ezgrpc2_free(message);
}
