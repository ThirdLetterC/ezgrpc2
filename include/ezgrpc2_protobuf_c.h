#pragma once

#include <protobuf-c/protobuf-c.h>
#include "ezgrpc2_call.h"
#include "ezgrpc2_defs.h"
#include "ezgrpc2_error.h"
#include "ezgrpc2_message.h"

/**
 * Returns a pointer to a ProtobufCAllocator whose alloc and free function pointers
 * route directly to ezgrpc2_malloc and ezgrpc2_free.
 * This guarantees that all memory allocated during protobuf-c unpacking
 * respects the active ezgrpc2 memory allocator (libc, mimalloc, jemalloc, or custom).
 */
EZGRPC2_API ProtobufCAllocator *ezgrpc2_pb_get_allocator(void);

/**
 * Unpacks a serialized protobuf-c message from an ezgrpc2_message.
 *
 * @param descriptor Pointer to the message type descriptor (e.g. &helloworld__hello_request__descriptor).
 * @param message The received gRPC message containing binary protobuf data.
 * @return A pointer to the unpacked ProtobufCMessage (cast to the appropriate struct type),
 *         or nullptr if unpacking failed or message was invalid.
 */
EZGRPC2_API ProtobufCMessage *ezgrpc2_pb_unpack(const ProtobufCMessageDescriptor *descriptor,
                                                const ezgrpc2_message *message);

/**
 * Helper macro for type-safe unpacking:
 *   Helloworld__HelloRequest *req = EZGRPC2_PB_UNPACK(
 *       Helloworld__HelloRequest, &helloworld__hello_request__descriptor, msg);
 */
#define EZGRPC2_PB_UNPACK(Type, descriptor, msg) ((Type *)ezgrpc2_pb_unpack((descriptor), (msg)))

/**
 * Serializes a ProtobufCMessage into a newly allocated ezgrpc2_message.
 *
 * @return The message, or nullptr on failure.
 */
EZGRPC2_API ezgrpc2_message *ezgrpc2_pb_pack(const ProtobufCMessage *message);

/**
 * Frees a message previously unpacked via ezgrpc2_pb_unpack.
 *
 * @param message The unpacked message to free.
 */
EZGRPC2_API void ezgrpc2_pb_free_unpacked(ProtobufCMessage *message);

/**
 * Helper macro for freeing an unpacked message:
 *   EZGRPC2_PB_FREE_UNPACKED(req);
 */
#define EZGRPC2_PB_FREE_UNPACKED(msg) ezgrpc2_pb_free_unpacked((ProtobufCMessage *)(msg))

/**
 * Serializes ``message`` and sends it on ``call``, like ezgrpc2_call_send().
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_call_send_pb(ezgrpc2_call *call, const ProtobufCMessage *message);
