#pragma once

#include "ezgrpc2_defs.h"

/**
 * The gRPC status a call ends with.
 * https://grpc.github.io/grpc/core/md_doc_statuscodes.html
 */
typedef enum ezgrpc2_status {
  EZGRPC2_STATUS_OK = 0,
  EZGRPC2_STATUS_CANCELLED = 1,
  EZGRPC2_STATUS_UNKNOWN = 2,
  EZGRPC2_STATUS_INVALID_ARGUMENT = 3,
  EZGRPC2_STATUS_DEADLINE_EXCEEDED = 4,
  EZGRPC2_STATUS_NOT_FOUND = 5,
  EZGRPC2_STATUS_ALREADY_EXISTS = 6,
  EZGRPC2_STATUS_PERMISSION_DENIED = 7,
  EZGRPC2_STATUS_RESOURCE_EXHAUSTED = 8,
  EZGRPC2_STATUS_FAILED_PRECONDITION = 9,
  EZGRPC2_STATUS_ABORTED = 10,
  EZGRPC2_STATUS_OUT_OF_RANGE = 11,
  EZGRPC2_STATUS_UNIMPLEMENTED = 12,
  EZGRPC2_STATUS_INTERNAL = 13,
  EZGRPC2_STATUS_UNAVAILABLE = 14,
  EZGRPC2_STATUS_DATA_LOSS = 15,
  EZGRPC2_STATUS_UNAUTHENTICATED = 16,
} ezgrpc2_status;

/**
 * Returns a short description of a status, e.g. ``"invalid argument"``. It is
 * the ``grpc-message`` of calls finished with an error and no message.
 */
EZGRPC2_API const char *ezgrpc2_status_str(ezgrpc2_status status);
