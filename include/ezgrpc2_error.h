#ifndef EZGRPC2_ERROR_H
#define EZGRPC2_ERROR_H

#include "ezgrpc2_defs.h"

/**
 * The result of a library call. ``EZGRPC2_OK`` is 0 and every error is
 * negative, so ``if (err < 0)`` and ``if (err != EZGRPC2_OK)`` both work.
 *
 * This is about the library call itself. The status a gRPC call ends with is
 * an :c:type:`ezgrpc2_status`.
 */
typedef enum ezgrpc2_err {
  EZGRPC2_OK = 0,
  /** An argument is invalid, e.g. a null pointer or an out of range value. */
  EZGRPC2_ERR_INVALID = -1,
  /** Out of memory. */
  EZGRPC2_ERR_NOMEM = -2,
  /** The call or its connection is gone: the client cancelled or disconnected. */
  EZGRPC2_ERR_CLOSED = -3,
  /** The call was already finished. */
  EZGRPC2_ERR_FINISHED = -4,
  /** Not allowed at this point, e.g. sending metadata after the first message. */
  EZGRPC2_ERR_STATE = -5,
  /** The HTTP/2 layer refused the operation. */
  EZGRPC2_ERR_PROTOCOL = -6,
  /** Already exists, e.g. a path that is registered twice. */
  EZGRPC2_ERR_EXISTS = -7,
  /** Not found. */
  EZGRPC2_ERR_NOT_FOUND = -8,
  /** The library was built without the feature (e.g. TLS). */
  EZGRPC2_ERR_UNSUPPORTED = -9,
  /** A system call failed. */
  EZGRPC2_ERR_IO = -10,
} ezgrpc2_err;

/**
 * Returns a static description of an error code.
 */
EZGRPC2_API const char *ezgrpc2_strerror(ezgrpc2_err err);

#endif /* EZGRPC2_ERROR_H */
