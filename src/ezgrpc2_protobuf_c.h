#ifndef EZGRPC2_PROTOBUF_C_H
#define EZGRPC2_PROTOBUF_C_H

#include <protobuf-c/protobuf-c.h>
#include "defs.h"
#include "ezgrpc2_message.h"
#include "ezgrpc2_messages.h"
#include "ezgrpc2_grpc_status.h"

#ifdef __cplusplus
extern "C" {
#endif

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
 * Serializes/packs a ProtobufCMessage into a newly-allocated ezgrpc2_message.
 *
 * @param message The ProtobufCMessage to pack.
 * @param compressed_flag gRPC compression flag (usually 0).
 * @return A newly allocated ezgrpc2_message ready to be sent, or nullptr on failure.
 */
EZGRPC2_API ezgrpc2_message *ezgrpc2_pb_pack(const ProtobufCMessage *message,
                                             uint8_t compressed_flag);

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
 * Convenience helper: packs a ProtobufCMessage and wraps it into a new ezgrpc2_messages
 * list ready to be passed to ezgrpc2_server_session_stream_send().
 *
 * @param message The ProtobufCMessage to serialize as the response.
 * @return A newly allocated ezgrpc2_messages list containing the packed message,
 *         or nullptr on failure.
 */
EZGRPC2_API ezgrpc2_messages *ezgrpc2_pb_create_response(const ProtobufCMessage *message);

#ifdef __cplusplus
}
#endif

#endif /* EZGRPC2_PROTOBUF_C_H */
