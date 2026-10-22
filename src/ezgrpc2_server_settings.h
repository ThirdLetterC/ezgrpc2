#ifndef EZGRPC2_SERVER_SETTINGS_H
#define EZGRPC2_SERVER_SETTINGS_H
#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include "defs.h"

typedef struct ezgrpc2_server_settings ezgrpc2_server_settings;
EZGRPC2_API void ezgrpc2_server_settings_set_max_connections(ezgrpc2_server_settings *server_settings, size_t max_connections);
EZGRPC2_API void ezgrpc2_server_settings_set_log_level(ezgrpc2_server_settings *server_settings, uint32_t logging_level);
EZGRPC2_API void ezgrpc2_server_settings_set_log_fp(ezgrpc2_server_settings *server_settings, FILE *fp);

EZGRPC2_API ezgrpc2_server_settings *ezgrpc2_server_settings_new(void *unused);
EZGRPC2_API void ezgrpc2_server_settings_free(ezgrpc2_server_settings *server_settings);
#endif
