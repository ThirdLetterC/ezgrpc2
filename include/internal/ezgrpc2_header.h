#ifndef EZGRPC2_HEADER_H
#define EZGRPC2_HEADER_H

#include <stddef.h>

/* A request header whose name and value point into the stream's nghttp2
 * buffers and are NUL-terminated. */
typedef struct ezgrpc2_header ezgrpc2_header;
struct ezgrpc2_header {
  size_t namelen;
  const char *name;
  size_t valuelen;
  const char *value;
};

#endif
