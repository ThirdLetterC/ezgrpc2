#include "core.h"
#include "ezgrpc2_error.h"
#include "ezgrpc2_status.h"

EZGRPC2_API const char *ezgrpc2_status_str(ezgrpc2_status status) {
  switch (status) {
  case EZGRPC2_STATUS_OK:
    return "ok";
  case EZGRPC2_STATUS_CANCELLED:
    return "cancelled";
  case EZGRPC2_STATUS_UNKNOWN:
    return "unknown";
  case EZGRPC2_STATUS_INVALID_ARGUMENT:
    return "invalid argument";
  case EZGRPC2_STATUS_DEADLINE_EXCEEDED:
    return "deadline exceeded";
  case EZGRPC2_STATUS_NOT_FOUND:
    return "not found";
  case EZGRPC2_STATUS_ALREADY_EXISTS:
    return "already exists";
  case EZGRPC2_STATUS_PERMISSION_DENIED:
    return "permission denied";
  case EZGRPC2_STATUS_UNAUTHENTICATED:
    return "unauthenticated";
  case EZGRPC2_STATUS_RESOURCE_EXHAUSTED:
    return "resource exhausted";
  case EZGRPC2_STATUS_FAILED_PRECONDITION:
    return "failed precondition";
  case EZGRPC2_STATUS_ABORTED:
    return "aborted";
  case EZGRPC2_STATUS_OUT_OF_RANGE:
    return "out of range";
  case EZGRPC2_STATUS_UNIMPLEMENTED:
    return "unimplemented";
  case EZGRPC2_STATUS_INTERNAL:
    return "internal";
  case EZGRPC2_STATUS_UNAVAILABLE:
    return "unavailable";
  case EZGRPC2_STATUS_DATA_LOSS:
    return "data loss";
  default:
    return "(null)";
  }
}

EZGRPC2_API const char *ezgrpc2_strerror(ezgrpc2_err err) {
  switch (err) {
  case EZGRPC2_OK:
    return "success";
  case EZGRPC2_ERR_INVALID:
    return "invalid argument";
  case EZGRPC2_ERR_NOMEM:
    return "out of memory";
  case EZGRPC2_ERR_CLOSED:
    return "the call or connection is closed";
  case EZGRPC2_ERR_FINISHED:
    return "the call is already finished";
  case EZGRPC2_ERR_STATE:
    return "not allowed in the current state";
  case EZGRPC2_ERR_PROTOCOL:
    return "HTTP/2 protocol error";
  case EZGRPC2_ERR_EXISTS:
    return "already exists";
  case EZGRPC2_ERR_NOT_FOUND:
    return "not found";
  case EZGRPC2_ERR_UNSUPPORTED:
    return "not supported by this build";
  case EZGRPC2_ERR_IO:
    return "I/O error";
  }
  return "unknown error";
}
