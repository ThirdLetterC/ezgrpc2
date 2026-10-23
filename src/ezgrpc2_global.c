#include <stdio.h>
#include "core.h"
#include "ezgrpc2_global.h"

#include "ezgrpc2_events_struct.h"

EZGRPC2_API int ezgrpc2_global_init([[maybe_unused]] uint64_t unused) {
#ifdef EZGRPC2_DEBUG
#define PRINT_SIZE(x) printf(#x ": %zu\n", sizeof(x))
  PRINT_SIZE(ezgrpc2_server);
  PRINT_SIZE(ezgrpc2_session);
  PRINT_SIZE(ezgrpc2_event);
  PRINT_SIZE(ezgrpc2_events);
  PRINT_SIZE(ezgrpc2_message);
#endif

  return 0;
}

EZGRPC2_API void ezgrpc2_global_cleanup(void) {}
