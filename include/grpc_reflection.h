#pragma once

#include "grpc_defs.h"
#include "grpc_error.h"

typedef struct grpc_server grpc_server;

#define GRPC_REFLECTION_V1ALPHA_PATH                                                            \
  "/grpc.reflection.v1alpha.ServerReflection/ServerReflectionInfo"
#define GRPC_REFLECTION_V1_PATH "/grpc.reflection.v1.ServerReflection/ServerReflectionInfo"

/**
 * Serves gRPC server reflection (v1 and v1alpha). The services are those of
 * the methods registered on the server.
 */
GRPC_API grpc_err grpc_reflection_enable(grpc_server *server);

/**
 * Adds a serialized FileDescriptorProto, returned for ``symbol_or_filename``,
 * so that tools like ``grpcurl describe`` can show the schema.
 */
GRPC_API grpc_err grpc_reflection_add_file_descriptor(grpc_server *server,
                                                               const char *symbol_or_filename,
                                                               const uint8_t *descriptor,
                                                               size_t len);
