#ifndef EZGRPC2_INTERCEPTOR_H
#define EZGRPC2_INTERCEPTOR_H

#include <stdint.h>
#include <stdbool.h>
#include "defs.h"
#include "ezgrpc2_server.h"
#include "ezgrpc2_event.h"
#include "ezgrpc2_messages.h"
#include "ezgrpc2_grpc_status.h"

typedef struct ezgrpc2_interceptor_info {
  ezgrpc2_session_uuid session_uuid;
  int32_t stream_id;
  const char *path;
  ezgrpc2_messages *messages;
  void *userdata;
} ezgrpc2_interceptor_info;

/**
 * Pre-call hook:
 * Returns EZGRPC2_GRPC_STATUS_OK to continue processing,
 * or an error status to reject the call immediately.
 * If rejected, custom_error_msg may be set to point to an error string.
 */
typedef ezgrpc2_grpc_status (*ezgrpc2_interceptor_pre_fn)(ezgrpc2_server *server,
                                                          const ezgrpc2_interceptor_info *info,
                                                          const char **custom_error_msg,
                                                          void *interceptor_data);

/**
 * Post-call hook:
 * Called after response messages are prepared (or stream ended).
 */
typedef void (*ezgrpc2_interceptor_post_fn)(ezgrpc2_server *server,
                                            const ezgrpc2_interceptor_info *info,
                                            ezgrpc2_grpc_status status, uint64_t duration_ms,
                                            void *interceptor_data);

typedef struct ezgrpc2_interceptor {
  const char *name;
  ezgrpc2_interceptor_pre_fn pre_fn;
  ezgrpc2_interceptor_post_fn post_fn;
  void *user_data;
} ezgrpc2_interceptor;

/**
 * Registers an interceptor on the server.
 * Multiple interceptors can be registered and are executed in registration order.
 * Returns 0 on success, non-zero on error.
 */
EZGRPC2_API int ezgrpc2_server_add_interceptor(ezgrpc2_server *server,
                                               const ezgrpc2_interceptor *interceptor);

/**
 * Executes registered pre-call interceptors for an incoming event.
 * If any interceptor rejects the call:
 *   - Automatically ends the stream with the returned error status and custom_error_msg.
 *   - Returns false (call was rejected).
 * If all interceptors return OK:
 *   - Returns true (proceed to method handler).
 */
EZGRPC2_API bool ezgrpc2_server_intercept_pre(ezgrpc2_server *server, ezgrpc2_event *event,
                                              const char *path);

/**
 * Executes registered post-call interceptors.
 */
EZGRPC2_API void ezgrpc2_server_intercept_post(ezgrpc2_server *server, ezgrpc2_event *event,
                                               const char *path, ezgrpc2_grpc_status status,
                                               uint64_t duration_ms);

/* Built-in Auth Interceptor Helper: Bearer Token Validator */
typedef bool (*ezgrpc2_token_validator_fn)(const char *token, void *user_data);

EZGRPC2_API ezgrpc2_interceptor
ezgrpc2_interceptor_create_bearer_auth(ezgrpc2_token_validator_fn validator, void *user_data);

#endif /* EZGRPC2_INTERCEPTOR_H */
