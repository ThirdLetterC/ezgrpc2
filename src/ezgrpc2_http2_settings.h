#ifndef EZGRPC2_HTTP2_SETTINGS_H
#define EZGRPC2_HTTP2_SETTINGS_H

#include <stddef.h>
#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ezgrpc2_http2_settings ezgrpc2_http2_settings;

EZGRPC2_API ezgrpc2_http2_settings *ezgrpc2_http2_settings_new(void *unused);
EZGRPC2_API void ezgrpc2_http2_settings_free(ezgrpc2_http2_settings *http2_settings);
EZGRPC2_API void ezgrpc2_http2_settings_copy(ezgrpc2_http2_settings *dst, const ezgrpc2_http2_settings *src);

/* Setters */
EZGRPC2_API void ezgrpc2_http2_settings_set_initial_window_size(ezgrpc2_http2_settings *http2_settings, size_t initial_window_size);
EZGRPC2_API void ezgrpc2_http2_settings_set_max_frame_size(ezgrpc2_http2_settings *http2_settings, size_t max_frame_size);
EZGRPC2_API void ezgrpc2_http2_settings_set_max_concurrent_streams(ezgrpc2_http2_settings *http2_settings, size_t max_concurrent_streams);
EZGRPC2_API void ezgrpc2_http2_settings_set_max_sessions(ezgrpc2_http2_settings *http2_settings, size_t max_sessions);
EZGRPC2_API void ezgrpc2_http2_settings_set_max_paths(ezgrpc2_http2_settings *http2_settings, size_t max_paths);

/* Getters */
EZGRPC2_API size_t ezgrpc2_http2_settings_get_initial_window_size(const ezgrpc2_http2_settings *http2_settings);
EZGRPC2_API size_t ezgrpc2_http2_settings_get_max_frame_size(const ezgrpc2_http2_settings *http2_settings);
EZGRPC2_API size_t ezgrpc2_http2_settings_get_max_concurrent_streams(const ezgrpc2_http2_settings *http2_settings);
EZGRPC2_API size_t ezgrpc2_http2_settings_get_max_sessions(const ezgrpc2_http2_settings *http2_settings);
EZGRPC2_API size_t ezgrpc2_http2_settings_get_max_paths(const ezgrpc2_http2_settings *http2_settings);

#ifdef __cplusplus
}
#endif

#endif /* EZGRPC2_HTTP2_SETTINGS_H */
