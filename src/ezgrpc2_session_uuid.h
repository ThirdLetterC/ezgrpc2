#ifndef EZGRPC2_SESSION_UUID_H
#define EZGRPC2_SESSION_UUID_H
#include "defs.h"
#include "ezgrpc2_session_uuid_struct.h"

typedef struct ezgrpc2_session_uuid ezgrpc2_session_uuid;

EZGRPC2_API int ezgrpc2_session_uuid_is_equal(ezgrpc2_session_uuid uuid1, ezgrpc2_session_uuid uuid2);
EZGRPC2_API ezgrpc2_session_uuid ezgrpc2_session_uuid_new(void *unused);
EZGRPC2_API ezgrpc2_session_uuid ezgrpc2_session_uuid_copy(ezgrpc2_session_uuid session_uuid);
EZGRPC2_API void ezgrpc2_session_uuid_free(ezgrpc2_session_uuid session_uuid);
#endif
