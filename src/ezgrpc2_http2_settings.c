#include <stdlib.h>
#include <string.h>
#include "ezgrpc2_http2_settings.h"
#include "ezgrpc2_http2_settings_struct.h"

#include <sys/socket.h>

static ezgrpc2_http2_settings default_http2_settings = {.initial_window_size = 1 << 20,
                                                        .max_frame_size = 16 * 1024,
                                                        .max_concurrent_streams = 1024,
                                                        .max_sessions = 1024,
                                                        .max_paths = 10};

EZGRPC2_API ezgrpc2_http2_settings *ezgrpc2_http2_settings_new([[maybe_unused]] void *unused) {
  ezgrpc2_http2_settings *http2_settings = malloc(sizeof(*http2_settings));
  if (http2_settings == nullptr)
    return nullptr;
  memcpy(http2_settings, &default_http2_settings, sizeof(ezgrpc2_http2_settings));
  return http2_settings;
}

EZGRPC2_API void ezgrpc2_http2_settings_free(ezgrpc2_http2_settings *http2_settings) {
  free(http2_settings);
}

EZGRPC2_API void ezgrpc2_http2_settings_copy(ezgrpc2_http2_settings *dst,
                                             const ezgrpc2_http2_settings *src) {
  if (dst != nullptr && src != nullptr) {
    memcpy(dst, src, sizeof(ezgrpc2_http2_settings));
  }
}

EZGRPC2_API void
ezgrpc2_http2_settings_set_initial_window_size(ezgrpc2_http2_settings *http2_settings,
                                               size_t initial_window_size) {
  if (http2_settings != nullptr) {
    http2_settings->initial_window_size = initial_window_size;
  }
}

EZGRPC2_API void ezgrpc2_http2_settings_set_max_frame_size(ezgrpc2_http2_settings *http2_settings,
                                                           size_t max_frame_size) {
  if (http2_settings != nullptr) {
    http2_settings->max_frame_size = max_frame_size;
  }
}

EZGRPC2_API void
ezgrpc2_http2_settings_set_max_concurrent_streams(ezgrpc2_http2_settings *http2_settings,
                                                  size_t max_concurrent_streams) {
  if (http2_settings != nullptr) {
    http2_settings->max_concurrent_streams = max_concurrent_streams;
  }
}

EZGRPC2_API void ezgrpc2_http2_settings_set_max_sessions(ezgrpc2_http2_settings *http2_settings,
                                                         size_t max_sessions) {
  if (http2_settings != nullptr) {
    http2_settings->max_sessions = max_sessions;
  }
}

EZGRPC2_API void ezgrpc2_http2_settings_set_max_paths(ezgrpc2_http2_settings *http2_settings,
                                                      size_t max_paths) {
  if (http2_settings != nullptr) {
    http2_settings->max_paths = max_paths;
  }
}

EZGRPC2_API size_t
ezgrpc2_http2_settings_get_initial_window_size(const ezgrpc2_http2_settings *http2_settings) {
  return http2_settings ? http2_settings->initial_window_size : 0;
}

EZGRPC2_API size_t
ezgrpc2_http2_settings_get_max_frame_size(const ezgrpc2_http2_settings *http2_settings) {
  return http2_settings ? http2_settings->max_frame_size : 0;
}

EZGRPC2_API size_t
ezgrpc2_http2_settings_get_max_concurrent_streams(const ezgrpc2_http2_settings *http2_settings) {
  return http2_settings ? http2_settings->max_concurrent_streams : 0;
}

EZGRPC2_API size_t
ezgrpc2_http2_settings_get_max_sessions(const ezgrpc2_http2_settings *http2_settings) {
  return http2_settings ? http2_settings->max_sessions : 0;
}

EZGRPC2_API size_t
ezgrpc2_http2_settings_get_max_paths(const ezgrpc2_http2_settings *http2_settings) {
  return http2_settings ? http2_settings->max_paths : 0;
}
