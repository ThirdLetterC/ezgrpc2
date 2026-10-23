#ifndef EZGRPC2_SESSION_UUID_STRUCT_H
#define EZGRPC2_SESSION_UUID_STRUCT_H

#include <uuid/uuid.h>

struct ezgrpc2_session_uuid {
  uuid_t uuid;
  size_t index;
};
#endif
