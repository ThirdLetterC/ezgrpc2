#include <stdlib.h>
#include <string.h>
#include "core.h"
#include "ezgrpc2_server_settings.h"
#include "ezgrpc2_server_settings_struct.h"

#include <sys/socket.h>

static ezgrpc2_server_settings default_server_settings = {
    .max_connections = 1024,
    .arena_events_size = sizeof(ezgrpc2_event) * 4096,
    .arena_messages_size = (sizeof(ezgrpc2_message) + 1024 * 1024 * 16),
    .logging_level = 0,
};

EZGRPC2_API ezgrpc2_server_settings *ezgrpc2_server_settings_new([[maybe_unused]] void *unused) {
  ezgrpc2_server_settings *server_settings = malloc(sizeof(*server_settings));
  if (server_settings == nullptr)
    return nullptr;
  memcpy(server_settings, &default_server_settings, sizeof(ezgrpc2_server_settings));
  server_settings->logging_fp = stderr;
  return server_settings;
}

EZGRPC2_API void ezgrpc2_server_settings_free(ezgrpc2_server_settings *server_settings) {
  free(server_settings);
}

EZGRPC2_API void ezgrpc2_server_settings_copy(ezgrpc2_server_settings *dst,
                                              const ezgrpc2_server_settings *src) {
  if (dst != nullptr && src != nullptr) {
    memcpy(dst, src, sizeof(ezgrpc2_server_settings));
  }
}

EZGRPC2_API void
ezgrpc2_server_settings_set_max_connections(ezgrpc2_server_settings *server_settings,
                                            size_t max_connections) {
  if (server_settings != nullptr) {
    server_settings->max_connections = max_connections;
  }
}

EZGRPC2_API void ezgrpc2_server_settings_set_log_fp(ezgrpc2_server_settings *server_settings,
                                                    FILE *fp) {
  if (server_settings != nullptr) {
    server_settings->logging_fp = fp;
  }
}

EZGRPC2_API void ezgrpc2_server_settings_set_log_level(ezgrpc2_server_settings *server_settings,
                                                       uint32_t logging_level) {
  if (server_settings != nullptr) {
    server_settings->logging_level = logging_level;
  }
}

EZGRPC2_API void
ezgrpc2_server_settings_set_arena_events_size(ezgrpc2_server_settings *server_settings,
                                              size_t size) {
  if (server_settings != nullptr) {
    server_settings->arena_events_size = size;
  }
}

EZGRPC2_API void
ezgrpc2_server_settings_set_arena_messages_size(ezgrpc2_server_settings *server_settings,
                                                size_t size) {
  if (server_settings != nullptr) {
    server_settings->arena_messages_size = size;
  }
}

EZGRPC2_API void ezgrpc2_server_settings_set_read_callback(ezgrpc2_server_settings *server_settings,
                                                           ssize_t (*read_cb)(void *sock, void *buf,
                                                                              size_t len,
                                                                              int flags)) {
  if (server_settings != nullptr) {
    server_settings->read_cb = read_cb;
  }
}

EZGRPC2_API void ezgrpc2_server_settings_set_write_callback(
    ezgrpc2_server_settings *server_settings,
    ssize_t (*write_cb)(void *sock, void *buf, size_t len, int flags)) {
  if (server_settings != nullptr) {
    server_settings->write_cb = write_cb;
  }
}

EZGRPC2_API size_t
ezgrpc2_server_settings_get_max_connections(const ezgrpc2_server_settings *server_settings) {
  return server_settings ? server_settings->max_connections : 0;
}

EZGRPC2_API uint32_t
ezgrpc2_server_settings_get_log_level(const ezgrpc2_server_settings *server_settings) {
  return server_settings ? server_settings->logging_level : 0;
}

EZGRPC2_API FILE *
ezgrpc2_server_settings_get_log_fp(const ezgrpc2_server_settings *server_settings) {
  return server_settings ? server_settings->logging_fp : nullptr;
}

EZGRPC2_API size_t
ezgrpc2_server_settings_get_arena_events_size(const ezgrpc2_server_settings *server_settings) {
  return server_settings ? server_settings->arena_events_size : 0;
}

EZGRPC2_API size_t
ezgrpc2_server_settings_get_arena_messages_size(const ezgrpc2_server_settings *server_settings) {
  return server_settings ? server_settings->arena_messages_size : 0;
}
