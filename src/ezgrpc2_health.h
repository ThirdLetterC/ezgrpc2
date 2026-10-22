#ifndef EZGRPC2_HEALTH_H
#define EZGRPC2_HEALTH_H

#include <stdint.h>
#include <stdbool.h>
#include "defs.h"
#include "ezgrpc2_server.h"
#include "ezgrpc2_event.h"

#ifdef __cplusplus
extern "C" {
#endif

#define EZGRPC2_HEALTH_PATH_CHECK "/grpc.health.v1.Health/Check"
#define EZGRPC2_HEALTH_PATH_WATCH "/grpc.health.v1.Health/Watch"

typedef enum {
  EZGRPC2_HEALTH_UNKNOWN = 0,
  EZGRPC2_HEALTH_SERVING = 1,
  EZGRPC2_HEALTH_NOT_SERVING = 2,
  EZGRPC2_HEALTH_SERVICE_UNKNOWN = 3,
} ezgrpc2_health_status;

/**
 * Registers the standard gRPC Health service (/grpc.health.v1.Health/Check)
 * on the provided server.
 * Returns 0 on success, non-zero on failure.
 */
EZGRPC2_API int ezgrpc2_health_register(ezgrpc2_server *server);

/**
 * Sets the serving status for a specific service name.
 * If service is empty string ("") or nullptr, sets the overall server status.
 */
EZGRPC2_API void ezgrpc2_health_set_status(const char *service, ezgrpc2_health_status status);

/**
 * Gets the current serving status for a specific service name.
 * If service is empty string ("") or nullptr, gets the overall server status.
 */
EZGRPC2_API ezgrpc2_health_status ezgrpc2_health_get_status(const char *service);

/**
 * Handles an EZGRPC2_EVENT_MESSAGE for the health check service.
 * Returns true if the event was handled by the health service, false otherwise.
 */
EZGRPC2_API bool ezgrpc2_health_handle_event(ezgrpc2_server *server, ezgrpc2_event *event);

#ifdef __cplusplus
}
#endif

#endif /* EZGRPC2_HEALTH_H */
