#pragma once

#include "grpc_defs.h"
#include "grpc_error.h"

typedef struct grpc_server grpc_server;

#define GRPC_HEALTH_PATH_CHECK "/grpc.health.v1.Health/Check"

typedef enum grpc_health_status {
  GRPC_HEALTH_UNKNOWN = 0,
  GRPC_HEALTH_SERVING = 1,
  GRPC_HEALTH_NOT_SERVING = 2,
  GRPC_HEALTH_SERVICE_UNKNOWN = 3,
} grpc_health_status;

/**
 * Serves the standard gRPC health check (/grpc.health.v1.Health/Check).
 * Services report SERVING until grpc_health_set_status() says otherwise.
 */
GRPC_API grpc_err grpc_health_enable(grpc_server *server);

/**
 * Sets the status of a service, or of the whole server when ``service`` is
 * nullptr or "". Safe to call from any thread.
 */
GRPC_API grpc_err grpc_health_set_status(grpc_server *server, const char *service,
                                                  grpc_health_status status);

/**
 * Returns the status of a service, or of the whole server when ``service`` is
 * nullptr or "". Safe to call from any thread.
 */
GRPC_API grpc_health_status grpc_health_get_status(grpc_server *server,
                                                            const char *service);
