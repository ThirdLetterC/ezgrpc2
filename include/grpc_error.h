#pragma once

#include "grpc_defs.h"

/**
 * The result of a library call. ``GRPC_OK`` is 0 and every error is
 * negative, so ``if (err < 0)`` and ``if (err != GRPC_OK)`` both work.
 *
 * This is about the library call itself. The status a gRPC call ends with is
 * an :c:type:`grpc_status`.
 */
typedef enum grpc_err {
  GRPC_OK = 0,
  /** An argument is invalid, e.g. a null pointer or an out of range value. */
  GRPC_ERR_INVALID = -1,
  /** Out of memory. */
  GRPC_ERR_NOMEM = -2,
  /** The call or its connection is gone: the client cancelled or disconnected. */
  GRPC_ERR_CLOSED = -3,
  /** The call was already finished. */
  GRPC_ERR_FINISHED = -4,
  /** Not allowed at this point, e.g. sending metadata after the first message. */
  GRPC_ERR_STATE = -5,
  /** The HTTP/2 layer refused the operation. */
  GRPC_ERR_PROTOCOL = -6,
  /** Already exists, e.g. a path that is registered twice. */
  GRPC_ERR_EXISTS = -7,
  /** Not found. */
  GRPC_ERR_NOT_FOUND = -8,
  /** The library was built without the feature (e.g. TLS). */
  GRPC_ERR_UNSUPPORTED = -9,
  /** A system call failed. */
  GRPC_ERR_IO = -10,
} grpc_err;

/**
 * Returns a static description of an error code.
 */
GRPC_API const char *grpc_strerror(grpc_err err);
