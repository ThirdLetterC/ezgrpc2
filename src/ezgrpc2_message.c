#include <stdlib.h>
#include <assert.h>
#include <string.h>
#include "core.h"
#include "ezgrpc2_message.h"

EZGRPC2_API ezgrpc2_message *ezgrpc2_message_new(uint8_t cflag, void *data, size_t data_len) {
  size_t size = sizeof(ezgrpc2_message) + data_len;
  ezgrpc2_message *message = aligned_alloc(CONFIG_CACHE_LINE_SIZE, REALIGN(CONFIG_CACHE_LINE_SIZE, size));
  if (message == nullptr)
    return nullptr;
  message->cflag = cflag;
  if (data_len && data)
    memcpy(message->data, data, data_len);
  message->len = (uint32_t)data_len;
  return message;
}

EZGRPC2_API ezgrpc2_message *ezgrpc2_message_new2(size_t data_len) {
  size_t size = sizeof(ezgrpc2_message) + data_len;
  ezgrpc2_message *message = aligned_alloc(CONFIG_CACHE_LINE_SIZE, REALIGN(CONFIG_CACHE_LINE_SIZE, size));
  if (message == nullptr)
    return nullptr;
  message->len = (uint32_t)data_len;
  message->cflag = 0;
  return message;
}

EZGRPC2_API void ezgrpc2_message_free(ezgrpc2_message *message) {
  free(message);
}
