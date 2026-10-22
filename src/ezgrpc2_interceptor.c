#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "ezgrpc2_interceptor.h"
#include "ezgrpc2_metadata.h"
#include "ezgrpc2_session.h"
#include "ezgrpc2_mem.h"
#include "core.h"

EZGRPC2_API int ezgrpc2_server_add_interceptor(
  ezgrpc2_server *server,
  const ezgrpc2_interceptor *interceptor) {
  if (server == nullptr || interceptor == nullptr) {
    return 1;
  }

  size_t new_nb = server->nb_interceptors + 1;
  ezgrpc2_interceptor *new_arr = ezgrpc2_realloc(server->interceptors, new_nb * sizeof(ezgrpc2_interceptor));
  if (new_arr == nullptr) {
    return 2;
  }

  server->interceptors = new_arr;
  server->interceptors[server->nb_interceptors] = *interceptor;
  server->nb_interceptors = new_nb;
  return 0;
}

EZGRPC2_API bool ezgrpc2_server_intercept_pre(
  ezgrpc2_server *server,
  ezgrpc2_event *event,
  const char *path) {
  if (server == nullptr || event == nullptr || server->nb_interceptors == 0) {
    return true;
  }

  ezgrpc2_interceptor_info info = {
    .session_uuid = event->session_uuid,
    .stream_id = event->message.stream_id,
    .path = path,
    .messages = event->message.messages,
    .userdata = event->userdata,
  };

  for (size_t i = 0; i < server->nb_interceptors; i++) {
    const ezgrpc2_interceptor *inc = &server->interceptors[i];
    if (inc->pre_fn != nullptr) {
      const char *custom_err = nullptr;
      ezgrpc2_grpc_status status = inc->pre_fn(server, &info, &custom_err, inc->user_data);
      if (status != EZGRPC2_GRPC_STATUS_OK) {
        ezgrpc2_server_session_stream_end2(
          server, event->session_uuid, event->message.stream_id, status,
          custom_err, nullptr, 0);
        return false;
      }
    }
  }

  return true;
}

EZGRPC2_API void ezgrpc2_server_intercept_post(
  ezgrpc2_server *server,
  ezgrpc2_event *event,
  const char *path,
  ezgrpc2_grpc_status status,
  uint64_t duration_ms) {
  if (server == nullptr || event == nullptr || server->nb_interceptors == 0) {
    return;
  }

  ezgrpc2_interceptor_info info = {
    .session_uuid = event->session_uuid,
    .stream_id = event->message.stream_id,
    .path = path,
    .messages = event->message.messages,
    .userdata = event->userdata,
  };

  for (size_t i = 0; i < server->nb_interceptors; i++) {
    const ezgrpc2_interceptor *inc = &server->interceptors[i];
    if (inc->post_fn != nullptr) {
      inc->post_fn(server, &info, status, duration_ms, inc->user_data);
    }
  }
}

typedef struct bearer_auth_data {
  ezgrpc2_token_validator_fn validator;
  void *user_data;
} bearer_auth_data;

static ezgrpc2_grpc_status bearer_auth_pre_hook(
  ezgrpc2_server *server,
  const ezgrpc2_interceptor_info *info,
  const char **custom_error_msg,
  void *interceptor_data) {
  bearer_auth_data *auth = interceptor_data;
  if (auth == nullptr || auth->validator == nullptr) {
    return EZGRPC2_GRPC_STATUS_OK;
  }

  size_t val_len = 0;
  const char *auth_hdr = ezgrpc2_metadata_get(server, info->session_uuid, info->stream_id, "authorization", &val_len);
  if (auth_hdr == nullptr) {
    *custom_error_msg = "missing authorization metadata";
    return EZGRPC2_GRPC_STATUS_UNAUTHENTICATED;
  }

  if (val_len < 7 || strncasecmp(auth_hdr, "Bearer ", 7) != 0) {
    *custom_error_msg = "invalid authorization header format (expected 'Bearer <token>')";
    return EZGRPC2_GRPC_STATUS_UNAUTHENTICATED;
  }

  const char *token = auth_hdr + 7;
  while (*token == ' ') token++;

  if (!auth->validator(token, auth->user_data)) {
    *custom_error_msg = "invalid or expired token";
    return EZGRPC2_GRPC_STATUS_UNAUTHENTICATED;
  }

  return EZGRPC2_GRPC_STATUS_OK;
}

EZGRPC2_API ezgrpc2_interceptor ezgrpc2_interceptor_create_bearer_auth(
  ezgrpc2_token_validator_fn validator,
  void *user_data) {
  bearer_auth_data *data = ezgrpc2_malloc(sizeof(*data));
  if (data != nullptr) {
    data->validator = validator;
    data->user_data = user_data;
  }

  ezgrpc2_interceptor inc = {
    .name = "bearer_auth",
    .pre_fn = bearer_auth_pre_hook,
    .post_fn = nullptr,
    .user_data = data,
  };
  return inc;
}
