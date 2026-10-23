#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include "core.h"
#include "ezgrpc2_timeout.h"

EZGRPC2_API ezgrpc2_err ezgrpc2_server_add_interceptor(ezgrpc2_server *server,
                                                       const ezgrpc2_interceptor *interceptor) {
  if (server == nullptr || interceptor == nullptr) {
    return EZGRPC2_ERR_INVALID;
  }

  size_t new_nb = server->nb_interceptors + 1;
  ezgrpc2_interceptor *new_arr =
      ezgrpc2_realloc(server->interceptors, new_nb * sizeof(ezgrpc2_interceptor));
  if (new_arr == nullptr) {
    return EZGRPC2_ERR_NOMEM;
  }

  server->interceptors = new_arr;
  server->interceptors[server->nb_interceptors] = *interceptor;
  server->nb_interceptors = new_nb;
  return EZGRPC2_OK;
}

bool interceptors_run_on_call(ezgrpc2_call *call) {
  ezgrpc2_server *server = call->server;
  for (size_t i = 0; i < server->nb_interceptors; i++) {
    const ezgrpc2_interceptor *inc = &server->interceptors[i];
    if (inc->on_call == nullptr)
      continue;
    const char *message = nullptr;
    ezgrpc2_status status = inc->on_call(call, &message, inc->userdata);
    if (status != EZGRPC2_STATUS_OK) {
      (void)ezgrpc2_call_finish(call, status, message);
      return false;
    }
  }
  return true;
}

void interceptors_run_on_done(ezgrpc2_call *call, ezgrpc2_status status) {
  /* once, and only for calls that went through on_call */
  if (call->done_reported || !call->started)
    return;
  call->done_reported = true;

  ezgrpc2_server *server = call->server;
  uint64_t duration_ms = ezgrpc2_time_now_ms() - call->start_ms;
  for (size_t i = 0; i < server->nb_interceptors; i++) {
    const ezgrpc2_interceptor *inc = &server->interceptors[i];
    if (inc->on_done != nullptr)
      inc->on_done(call, status, duration_ms, inc->userdata);
  }
}

void interceptors_free(ezgrpc2_server *server) {
  for (size_t i = 0; i < server->nb_interceptors; i++) {
    const ezgrpc2_interceptor *inc = &server->interceptors[i];
    if (inc->userdata_free != nullptr)
      inc->userdata_free(inc->userdata);
  }
  ezgrpc2_free(server->interceptors);
  server->interceptors = nullptr;
  server->nb_interceptors = 0;
}

typedef struct bearer_auth_data {
  ezgrpc2_token_validator_fn validator;
  void *userdata;
} bearer_auth_data;

static ezgrpc2_status bearer_auth_on_call(ezgrpc2_call *call, const char **message,
                                          void *userdata) {
  bearer_auth_data *auth = userdata;
  if (auth == nullptr || auth->validator == nullptr) {
    return EZGRPC2_STATUS_OK;
  }

  size_t len = 0;
  const char *header = ezgrpc2_call_metadata(call, "authorization", &len);
  if (header == nullptr) {
    *message = "missing authorization metadata";
    return EZGRPC2_STATUS_UNAUTHENTICATED;
  }
  if (len < 7 || strncasecmp(header, "Bearer ", 7) != 0) {
    *message = "invalid authorization header format (expected 'Bearer <token>')";
    return EZGRPC2_STATUS_UNAUTHENTICATED;
  }

  const char *token = header + 7;
  while (*token == ' ')
    token++;

  if (!auth->validator(token, auth->userdata)) {
    *message = "invalid or expired token";
    return EZGRPC2_STATUS_UNAUTHENTICATED;
  }
  return EZGRPC2_STATUS_OK;
}

EZGRPC2_API ezgrpc2_interceptor ezgrpc2_interceptor_bearer_auth(ezgrpc2_token_validator_fn validator,
                                                                void *userdata) {
  bearer_auth_data *data = ezgrpc2_malloc(sizeof(*data));
  if (data != nullptr) {
    data->validator = validator;
    data->userdata = userdata;
  }

  return (ezgrpc2_interceptor){
      .name = "bearer_auth",
      .on_call = bearer_auth_on_call,
      .userdata = data,
      .userdata_free = ezgrpc2_free,
  };
}
