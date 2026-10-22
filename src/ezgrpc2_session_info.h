#ifndef EZGRPC2_SESSION_INFO_H
#define EZGRPC2_SESSION_INFO_H

#include <stdint.h>
#include <stddef.h>
#include "ezgrpc2_session_uuid.h"

typedef struct ezgrpc2_session_info ezgrpc2_session_info;

struct ezgrpc2_session_info {
  ezgrpc2_session_uuid *session_uuid;
  size_t nb_open_streams;
  uint64_t time_since_last_request;
};

ezgrpc2_session_info *session_info_new(void *unused);
void session_info_free(ezgrpc2_session_info *info);

#endif
