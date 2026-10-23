#pragma once

#include <stddef.h>

/* A request header whose name and value point into the stream's nghttp2
 * buffers and are NUL-terminated. */
typedef struct grpc_header grpc_header;
struct grpc_header {
  size_t namelen;
  const char *name;
  size_t valuelen;
  const char *value;
};
