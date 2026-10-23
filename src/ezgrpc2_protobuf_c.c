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

EZGRPC2_API ezgrpc2_message *ezgrpc2_pb_pack(const ProtobufCMessage *message) {
  if (message == nullptr) {
    return nullptr;
  }

  size_t packed_size = protobuf_c_message_get_packed_size(message);
  ezgrpc2_message *msg = ezgrpc2_message_alloc(packed_size);
  if (msg == nullptr) {
    return nullptr;
  }

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

EZGRPC2_API ezgrpc2_err ezgrpc2_call_send_pb(ezgrpc2_call *call, const ProtobufCMessage *message) {
  if (call == nullptr || message == nullptr) {
    return EZGRPC2_ERR_INVALID;
  }
  ezgrpc2_message *msg = ezgrpc2_pb_pack(message);
  if (msg == nullptr) {
    return EZGRPC2_ERR_NOMEM;
  }
  return ezgrpc2_call_send(call, msg);
}
