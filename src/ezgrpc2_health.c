#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "core.h"

#define MAX_SERVICE_NAME_LEN 128

typedef struct service_health_entry {
  char *name;
  ezgrpc2_health_status status;
} service_health_entry;

/* per server. guarded by `lock`, since statuses may be set from any thread */
struct health_state {
  pthread_mutex_t lock;
  ezgrpc2_health_status overall;
  service_health_entry *entries;
  size_t nb_entries;
};

static struct health_state *health_state_get(ezgrpc2_server *server) {
  if (server->health == nullptr) {
    struct health_state *health = ezgrpc2_calloc(1, sizeof(*health));
    if (health == nullptr)
      return nullptr;
    pthread_mutex_init(&health->lock, nullptr);
    health->overall = EZGRPC2_HEALTH_SERVING;
    server->health = health;
  }
  return server->health;
}

void health_free(ezgrpc2_server *server) {
  struct health_state *health = server->health;
  if (health == nullptr)
    return;
  for (size_t i = 0; i < health->nb_entries; i++)
    ezgrpc2_free(health->entries[i].name);
  ezgrpc2_free(health->entries);
  pthread_mutex_destroy(&health->lock);
  ezgrpc2_free(health);
  server->health = nullptr;
}

EZGRPC2_API ezgrpc2_err ezgrpc2_health_set_status(ezgrpc2_server *server, const char *service,
                                                  ezgrpc2_health_status status) {
  if (server == nullptr)
    return EZGRPC2_ERR_INVALID;
  /* created on first use; set statuses before other threads use the server */
  struct health_state *health = health_state_get(server);
  if (health == nullptr)
    return EZGRPC2_ERR_NOMEM;

  ezgrpc2_err err = EZGRPC2_OK;
  pthread_mutex_lock(&health->lock);
  if (service == nullptr || *service == '\0') {
    health->overall = status;
    goto out;
  }
  for (size_t i = 0; i < health->nb_entries; i++) {
    if (strcmp(health->entries[i].name, service) == 0) {
      health->entries[i].status = status;
      goto out;
    }
  }
  service_health_entry *entries =
      ezgrpc2_realloc(health->entries, (health->nb_entries + 1) * sizeof(*entries));
  char *name = ezgrpc2_strndup(service, MAX_SERVICE_NAME_LEN - 1);
  if (entries != nullptr)
    health->entries = entries;
  if (entries == nullptr || name == nullptr) {
    ezgrpc2_free(name);
    err = EZGRPC2_ERR_NOMEM;
    goto out;
  }
  health->entries[health->nb_entries++] = (service_health_entry){name, status};
out:
  pthread_mutex_unlock(&health->lock);
  return err;
}

EZGRPC2_API ezgrpc2_health_status ezgrpc2_health_get_status(ezgrpc2_server *server,
                                                            const char *service) {
  if (server == nullptr || server->health == nullptr)
    return EZGRPC2_HEALTH_UNKNOWN;
  struct health_state *health = server->health;

  /* services report SERVING until told otherwise */
  ezgrpc2_health_status res = EZGRPC2_HEALTH_SERVING;
  pthread_mutex_lock(&health->lock);
  if (service == nullptr || *service == '\0') {
    res = health->overall;
  } else {
    for (size_t i = 0; i < health->nb_entries; i++) {
      if (strcmp(health->entries[i].name, service) == 0) {
        res = health->entries[i].status;
        break;
      }
    }
  }
  pthread_mutex_unlock(&health->lock);
  return res;
}

static void parse_health_request_service(const uint8_t *data, size_t len, char *out_service,
                                         size_t max_len) {
  out_service[0] = '\0';
  if (len == 0 || data == nullptr) {
    return;
  }

  // Protobuf wire format: Field 1, type 2 (length-delimited): tag = 0x0A
  size_t idx = 0;
  while (idx < len) {
    uint8_t tag = data[idx++];
    uint8_t field_num = tag >> 3;
    uint8_t wire_type = tag & 0x07;

    if (field_num == 1 && wire_type == 2) {
      // Varint length
      size_t str_len = 0;
      size_t shift = 0;
      while (idx < len) {
        uint8_t b = data[idx++];
        str_len |= (size_t)(b & 0x7F) << shift;
        if (!(b & 0x80))
          break;
        shift += 7;
      }
      if (idx + str_len <= len) {
        size_t cpy = str_len < max_len - 1 ? str_len : max_len - 1;
        memcpy(out_service, data + idx, cpy);
        out_service[cpy] = '\0';
      }
      return;
    } else if (wire_type == 0) {
      // varint
      while (idx < len && (data[idx++] & 0x80)) {
      }
    } else if (wire_type == 2) {
      // skip length-delimited
      size_t skip_len = 0;
      size_t shift = 0;
      while (idx < len) {
        uint8_t b = data[idx++];
        skip_len |= (size_t)(b & 0x7F) << shift;
        if (!(b & 0x80))
          break;
        shift += 7;
      }
      idx += skip_len;
    } else {
      break;
    }
  }
}

/* /grpc.health.v1.Health/Check */
static void health_check(ezgrpc2_call *call, const ezgrpc2_message *request, void *userdata) {
  ezgrpc2_server *server = userdata;
  char service_name[MAX_SERVICE_NAME_LEN] = {};
  if (request->len > 0) {
    parse_health_request_service(request->data, request->len, service_name,
                                 sizeof(service_name));
  }

  ezgrpc2_health_status status = ezgrpc2_health_get_status(server, service_name);

  /* HealthCheckResponse { ServingStatus status = 1; } */
  uint8_t response[2] = {0x08, (uint8_t)status};
  (void)ezgrpc2_call_send_data(call, response, sizeof(response));
  (void)ezgrpc2_call_finish(call, EZGRPC2_STATUS_OK, nullptr);
}

EZGRPC2_API ezgrpc2_err ezgrpc2_health_enable(ezgrpc2_server *server) {
  if (server == nullptr)
    return EZGRPC2_ERR_INVALID;
  if (health_state_get(server) == nullptr)
    return EZGRPC2_ERR_NOMEM;
  return ezgrpc2_server_handle(server, EZGRPC2_HEALTH_PATH_CHECK, health_check, server);
}
