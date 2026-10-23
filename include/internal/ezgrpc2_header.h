#ifndef EZGRPC2_HEADER_H
#define EZGRPC2_HEADER_H

#include <stddef.h>

/* a request header. name and value point into nghttp2 buffers held by the
 * stream, and are NULL-terminated */
typedef struct ezgrpc2_header ezgrpc2_header;
struct ezgrpc2_header {
  size_t namelen;
  const char *name;
  size_t valuelen;
  const char *value;
};

#endif
