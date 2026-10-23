#include "ezgrpc2_protobuf_c.h"
#include "ezgrpc2_mem.h"

static void *pb_alloc([[maybe_unused]] void *allocator_data, size_t size) {
  return ezgrpc2_malloc(size);
}

static void pb_free([[maybe_unused]] void *allocator_data, void *pointer) {
  ezgrpc2_free(pointer);
}

static ProtobufCAllocator g_ezgrpc2_pb_allocator = {
    .alloc = pb_alloc,
    .free = pb_free,
    .allocator_data = nullptr,
};

EZGRPC2_API ProtobufCAllocator *ezgrpc2_pb_get_allocator(void) {
  return &g_ezgrpc2_pb_allocator;
}

EZGRPC2_API ProtobufCMessage *ezgrpc2_pb_unpack(const ProtobufCMessageDescriptor *descriptor,
                                                const ezgrpc2_message *message) {
  if (descriptor == nullptr || message == nullptr) {
    return nullptr;
  }
  return protobuf_c_message_unpack(descriptor, &g_ezgrpc2_pb_allocator, message->len,
                                   message->data);
}

EZGRPC2_API ezgrpc2_message *ezgrpc2_pb_pack(const ProtobufCMessage *message,
                                             uint8_t compressed_flag) {
  if (message == nullptr) {
    return nullptr;
  }

  size_t packed_size = protobuf_c_message_get_packed_size(message);
  ezgrpc2_message *msg = ezgrpc2_message_new2(packed_size);
  if (msg == nullptr) {
    return nullptr;
  }

  msg->cflag = compressed_flag;
  size_t written = protobuf_c_message_pack(message, msg->data);
  if (written != packed_size) {
    ezgrpc2_message_free(msg);
    return nullptr;
  }

  return msg;
}

EZGRPC2_API void ezgrpc2_pb_free_unpacked(ProtobufCMessage *message) {
  if (message != nullptr) {
    protobuf_c_message_free_unpacked(message, &g_ezgrpc2_pb_allocator);
  }
}

EZGRPC2_API ezgrpc2_messages *ezgrpc2_pb_create_response(const ProtobufCMessage *message) {
  if (message == nullptr) {
    return nullptr;
  }

  ezgrpc2_message *msg = ezgrpc2_pb_pack(message, 0);
  if (msg == nullptr) {
    return nullptr;
  }

  ezgrpc2_messages *msgs = ezgrpc2_messages_new(nullptr);
  if (msgs == nullptr) {
    ezgrpc2_message_free(msg);
    return nullptr;
  }

  ezgrpc2_messages_push(msgs, msg);
  return msgs;
}
