#include "core.h"
#include "ezgrpc2_session_info.h"

ezgrpc2_session_info *session_info_new([[maybe_unused]] void *unused) {
  ezgrpc2_session_info *session_info = calloc(1, sizeof(*session_info));
  return session_info;
}

void session_info_free(ezgrpc2_session_info *info) {
  free(info);
}
