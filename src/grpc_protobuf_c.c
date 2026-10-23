#include "grpc_protobuf_c.h"
#include "grpc_mem.h"

static void *pb_alloc([[maybe_unused]] void *allocator_data, size_t size) {
  return grpc_malloc(size);
}

static void pb_free([[maybe_unused]] void *allocator_data, void *pointer) {
  grpc_free(pointer);
}

static ProtobufCAllocator g_grpc_pb_allocator = {
    .alloc = pb_alloc,
    .free = pb_free,
    .allocator_data = nullptr,
};

GRPC_API ProtobufCAllocator *grpc_pb_get_allocator(void) {
  return &g_grpc_pb_allocator;
}

GRPC_API ProtobufCMessage *grpc_pb_unpack(const ProtobufCMessageDescriptor *descriptor,
                                                const grpc_message *message) {
  if (descriptor == nullptr || message == nullptr) {
    return nullptr;
  }
  return protobuf_c_message_unpack(descriptor, &g_grpc_pb_allocator, message->len,
                                   message->data);
}

GRPC_API grpc_message *grpc_pb_pack(const ProtobufCMessage *message) {
  if (message == nullptr) {
    return nullptr;
  }

  size_t packed_size = protobuf_c_message_get_packed_size(message);
  grpc_message *msg = grpc_message_alloc(packed_size);
  if (msg == nullptr) {
    return nullptr;
  }

  size_t written = protobuf_c_message_pack(message, msg->data);
  if (written != packed_size) {
    grpc_message_free(msg);
    return nullptr;
  }

  return msg;
}

GRPC_API void grpc_pb_free_unpacked(ProtobufCMessage *message) {
  if (message != nullptr) {
    protobuf_c_message_free_unpacked(message, &g_grpc_pb_allocator);
  }
}

GRPC_API grpc_err grpc_call_send_pb(grpc_call *call, const ProtobufCMessage *message) {
  if (call == nullptr || message == nullptr) {
    return GRPC_ERR_INVALID;
  }
  grpc_message *msg = grpc_pb_pack(message);
  if (msg == nullptr) {
    return GRPC_ERR_NOMEM;
  }
  return grpc_call_send(call, msg);
}
