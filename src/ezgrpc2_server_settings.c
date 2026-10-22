#include <stdlib.h>
#include <string.h>
#include "core.h"
#include "ezgrpc2_server_settings.h"
#include "ezgrpc2_server_settings_struct.h"

#ifdef _WIN32
#  include <ws2tcpip.h>
#  include <winsock2.h>
#  include <windows.h>
#else
#include <sys/socket.h>
#endif

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

EZGRPC2_API void ezgrpc2_server_settings_set_max_connections(ezgrpc2_server_settings *server_settings, size_t max_connections) {
  server_settings->max_connections = max_connections;
}

EZGRPC2_API void ezgrpc2_server_settings_set_log_fp(ezgrpc2_server_settings *server_settings, FILE *fp) {
  server_settings->logging_fp = fp;
}

EZGRPC2_API void ezgrpc2_server_settings_set_log_level(ezgrpc2_server_settings *server_settings, uint32_t logging_level) {
  server_settings->logging_level = logging_level;
}

EZGRPC2_API void ezgrpc2_server_settings_free(ezgrpc2_server_settings *server_settings) {
  free(server_settings);
}
