#pragma once

#include <protobuf-c/protobuf-c.h>
#include "grpc_call.h"
#include "grpc_defs.h"
#include "grpc_error.h"
#include "grpc_message.h"

/**
 * Returns a pointer to a ProtobufCAllocator whose alloc and free function pointers
 * route directly to grpc_malloc and grpc_free.
 * This guarantees that all memory allocated during protobuf-c unpacking
 * respects the active grpc memory allocator (libc, mimalloc, jemalloc, or custom).
 */
GRPC_API ProtobufCAllocator *grpc_pb_get_allocator(void);

/**
 * Unpacks a serialized protobuf-c message from an grpc_message.
 *
 * @param descriptor Pointer to the message type descriptor (e.g. &helloworld__hello_request__descriptor).
 * @param message The received gRPC message containing binary protobuf data.
 * @return A pointer to the unpacked ProtobufCMessage (cast to the appropriate struct type),
 *         or nullptr if unpacking failed or message was invalid.
 */
GRPC_API ProtobufCMessage *grpc_pb_unpack(const ProtobufCMessageDescriptor *descriptor,
                                                const grpc_message *message);

/**
 * Helper macro for type-safe unpacking:
 *   Helloworld__HelloRequest *req = GRPC_PB_UNPACK(
 *       Helloworld__HelloRequest, &helloworld__hello_request__descriptor, msg);
 */
#define GRPC_PB_UNPACK(Type, descriptor, msg) ((Type *)grpc_pb_unpack((descriptor), (msg)))

/**
 * Serializes a ProtobufCMessage into a newly allocated grpc_message.
 *
 * @return The message, or nullptr on failure.
 */
GRPC_API grpc_message *grpc_pb_pack(const ProtobufCMessage *message);

/**
 * Frees a message previously unpacked via grpc_pb_unpack.
 *
 * @param message The unpacked message to free.
 */
GRPC_API void grpc_pb_free_unpacked(ProtobufCMessage *message);

/**
 * Helper macro for freeing an unpacked message:
 *   GRPC_PB_FREE_UNPACKED(req);
 */
#define GRPC_PB_FREE_UNPACKED(msg) grpc_pb_free_unpacked((ProtobufCMessage *)(msg))

/**
 * Serializes ``message`` and sends it on ``call``, like grpc_call_send().
 */
GRPC_API grpc_err grpc_call_send_pb(grpc_call *call, const ProtobufCMessage *message);
