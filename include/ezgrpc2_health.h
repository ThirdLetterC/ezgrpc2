#pragma once

#include "ezgrpc2_defs.h"
#include "ezgrpc2_error.h"

typedef struct ezgrpc2_server ezgrpc2_server;

#define EZGRPC2_HEALTH_PATH_CHECK "/grpc.health.v1.Health/Check"

typedef enum ezgrpc2_health_status {
  EZGRPC2_HEALTH_UNKNOWN = 0,
  EZGRPC2_HEALTH_SERVING = 1,
  EZGRPC2_HEALTH_NOT_SERVING = 2,
  EZGRPC2_HEALTH_SERVICE_UNKNOWN = 3,
} ezgrpc2_health_status;

/**
 * Serves the standard gRPC health check (/grpc.health.v1.Health/Check).
 * Services report SERVING until ezgrpc2_health_set_status() says otherwise.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_health_enable(ezgrpc2_server *server);

/**
 * Sets the status of a service, or of the whole server when ``service`` is
 * nullptr or "". Safe to call from any thread.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_health_set_status(ezgrpc2_server *server, const char *service,
                                                  ezgrpc2_health_status status);

/**
 * Returns the status of a service, or of the whole server when ``service`` is
 * nullptr or "". Safe to call from any thread.
 */
EZGRPC2_API ezgrpc2_health_status ezgrpc2_health_get_status(ezgrpc2_server *server,
                                                            const char *service);
