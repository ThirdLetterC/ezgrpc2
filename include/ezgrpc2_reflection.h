#ifndef EZGRPC2_REFLECTION_H
#define EZGRPC2_REFLECTION_H

#include "ezgrpc2_defs.h"
#include "ezgrpc2_error.h"

typedef struct ezgrpc2_server ezgrpc2_server;

#define EZGRPC2_REFLECTION_V1ALPHA_PATH                                                            \
  "/grpc.reflection.v1alpha.ServerReflection/ServerReflectionInfo"
#define EZGRPC2_REFLECTION_V1_PATH "/grpc.reflection.v1.ServerReflection/ServerReflectionInfo"

/**
 * Serves gRPC server reflection (v1 and v1alpha). The services are those of
 * the methods registered on the server.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_reflection_enable(ezgrpc2_server *server);

/**
 * Adds a serialized FileDescriptorProto, returned for ``symbol_or_filename``,
 * so that tools like ``grpcurl describe`` can show the schema.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_reflection_add_file_descriptor(ezgrpc2_server *server,
                                                               const char *symbol_or_filename,
                                                               const uint8_t *descriptor,
                                                               size_t len);

#endif /* EZGRPC2_REFLECTION_H */
