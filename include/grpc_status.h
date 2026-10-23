#pragma once

#include "grpc_defs.h"

/**
 * The gRPC status a call ends with.
 * https://grpc.github.io/grpc/core/md_doc_statuscodes.html
 */
typedef enum grpc_status {
  GRPC_STATUS_OK = 0,
  GRPC_STATUS_CANCELLED = 1,
  GRPC_STATUS_UNKNOWN = 2,
  GRPC_STATUS_INVALID_ARGUMENT = 3,
  GRPC_STATUS_DEADLINE_EXCEEDED = 4,
  GRPC_STATUS_NOT_FOUND = 5,
  GRPC_STATUS_ALREADY_EXISTS = 6,
  GRPC_STATUS_PERMISSION_DENIED = 7,
  GRPC_STATUS_RESOURCE_EXHAUSTED = 8,
  GRPC_STATUS_FAILED_PRECONDITION = 9,
  GRPC_STATUS_ABORTED = 10,
  GRPC_STATUS_OUT_OF_RANGE = 11,
  GRPC_STATUS_UNIMPLEMENTED = 12,
  GRPC_STATUS_INTERNAL = 13,
  GRPC_STATUS_UNAVAILABLE = 14,
  GRPC_STATUS_DATA_LOSS = 15,
  GRPC_STATUS_UNAUTHENTICATED = 16,
} grpc_status;

/**
 * Returns a short description of a status, e.g. ``"invalid argument"``. It is
 * the ``grpc-message`` of calls finished with an error and no message.
 */
GRPC_API const char *grpc_status_str(grpc_status status);
