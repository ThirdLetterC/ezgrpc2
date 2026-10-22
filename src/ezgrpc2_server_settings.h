#ifndef EZGRPC2_SERVER_SETTINGS_H
#define EZGRPC2_SERVER_SETTINGS_H

#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <stddef.h>
#include <sys/types.h>
#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ezgrpc2_server_settings ezgrpc2_server_settings;

EZGRPC2_API ezgrpc2_server_settings *ezgrpc2_server_settings_new(void *unused);
EZGRPC2_API void ezgrpc2_server_settings_free(ezgrpc2_server_settings *server_settings);
EZGRPC2_API void ezgrpc2_server_settings_copy(ezgrpc2_server_settings *dst, const ezgrpc2_server_settings *src);

/* Setters */
EZGRPC2_API void ezgrpc2_server_settings_set_max_connections(ezgrpc2_server_settings *server_settings, size_t max_connections);
EZGRPC2_API void ezgrpc2_server_settings_set_log_level(ezgrpc2_server_settings *server_settings, uint32_t logging_level);
EZGRPC2_API void ezgrpc2_server_settings_set_log_fp(ezgrpc2_server_settings *server_settings, FILE *fp);
EZGRPC2_API void ezgrpc2_server_settings_set_arena_events_size(ezgrpc2_server_settings *server_settings, size_t size);
EZGRPC2_API void ezgrpc2_server_settings_set_arena_messages_size(ezgrpc2_server_settings *server_settings, size_t size);
EZGRPC2_API void ezgrpc2_server_settings_set_read_callback(ezgrpc2_server_settings *server_settings, ssize_t (*read_cb)(void *sock, void *buf, size_t len, int flags));
EZGRPC2_API void ezgrpc2_server_settings_set_write_callback(ezgrpc2_server_settings *server_settings, ssize_t (*write_cb)(void *sock, void *buf, size_t len, int flags));

/* Getters */
EZGRPC2_API size_t ezgrpc2_server_settings_get_max_connections(const ezgrpc2_server_settings *server_settings);
EZGRPC2_API uint32_t ezgrpc2_server_settings_get_log_level(const ezgrpc2_server_settings *server_settings);
EZGRPC2_API FILE *ezgrpc2_server_settings_get_log_fp(const ezgrpc2_server_settings *server_settings);
EZGRPC2_API size_t ezgrpc2_server_settings_get_arena_events_size(const ezgrpc2_server_settings *server_settings);
EZGRPC2_API size_t ezgrpc2_server_settings_get_arena_messages_size(const ezgrpc2_server_settings *server_settings);

#ifdef __cplusplus
}
#endif

#endif /* EZGRPC2_SERVER_SETTINGS_H */
