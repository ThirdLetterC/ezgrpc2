#ifndef EZGRPC2_REFLECTION_H
#define EZGRPC2_REFLECTION_H

#include <stdint.h>
#include <stdbool.h>
#include <stddef.h>
#include "defs.h"
#include "ezgrpc2_server.h"
#include "ezgrpc2_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EZGRPC2_REFLECTION_V1ALPHA_PATH \
  "/grpc.reflection.v1alpha.ServerReflection/ServerReflectionInfo"
#define EZGRPC2_REFLECTION_V1_PATH "/grpc.reflection.v1.ServerReflection/ServerReflectionInfo"

/**
 * Registers the gRPC Server Reflection service (v1alpha and v1) on the server.
 * Automatically discovers all registered service paths on the server.
 * Returns 0 on success, non-zero on error.
 */
EZGRPC2_API int ezgrpc2_reflection_register(ezgrpc2_server *server);

/**
 * Registers raw serialized FileDescriptorProto bytes for a given filename or symbol.
 * Allows tools like 'grpcurl describe' to inspect schemas dynamically.
 * Returns 0 on success, non-zero on error.
 */
EZGRPC2_API int ezgrpc2_reflection_add_file_descriptor(const char *symbol_or_filename,
                                                       const uint8_t *descriptor_bytes,
                                                       size_t descriptor_len);

/**
 * Handles an EZGRPC2_EVENT_MESSAGE for the reflection service.
 * Returns true if the event was handled by the reflection service, false otherwise.
 */
EZGRPC2_API bool ezgrpc2_reflection_handle_event(ezgrpc2_server *server, ezgrpc2_event *event);

#ifdef __cplusplus
}
#endif

#endif /* EZGRPC2_REFLECTION_H */
