#include <stdlib.h>
#include <assert.h>

#include "ezgrpc2_session_uuid.h"

EZGRPC2_API int ezgrpc2_session_uuid_is_equal(ezgrpc2_session_uuid uuid1, ezgrpc2_session_uuid uuid2) {
#ifdef _WIN32
  RPC_STATUS res;
  return (uuid1.index == uuid2.index && !UuidCompare(&uuid1.uuid, &uuid2.uuid, &res));
#else
  return (uuid1.index == uuid2.index && !uuid_compare(uuid1.uuid, uuid2.uuid));
#endif
}

EZGRPC2_API ezgrpc2_session_uuid ezgrpc2_session_uuid_new([[maybe_unused]] void *unused) {
  ezgrpc2_session_uuid session_uuid = {};
#ifdef _WIN32
  RPC_STATUS res = UuidCreate(&session_uuid.uuid);
  assert(res == RPC_S_OK);
#else
  uuid_generate_random(session_uuid.uuid);
#endif
  return session_uuid;
}

EZGRPC2_API ezgrpc2_session_uuid ezgrpc2_session_uuid_copy(ezgrpc2_session_uuid session_uuid) {
  return session_uuid;
}

EZGRPC2_API void ezgrpc2_session_uuid_free([[maybe_unused]] ezgrpc2_session_uuid session_uuid) {
}
