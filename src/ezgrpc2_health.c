#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "ezgrpc2_health.h"
#include "ezgrpc2_session.h"
#include "ezgrpc2_message.h"
#include "ezgrpc2_messages.h"
#include "ezgrpc2_grpc_status.h"

#define MAX_HEALTH_SERVICES 32
#define MAX_SERVICE_NAME_LEN 128

typedef struct {
  char name[MAX_SERVICE_NAME_LEN];
  ezgrpc2_health_status status;
} service_health_entry;

static service_health_entry s_health_entries[MAX_HEALTH_SERVICES];
static size_t s_health_entries_count = 0;
static ezgrpc2_health_status s_overall_status = EZGRPC2_HEALTH_SERVING;
static pthread_mutex_t s_health_lock = PTHREAD_MUTEX_INITIALIZER;

static const int s_health_userdata_marker = 0x4845414C; // "HEAL"

EZGRPC2_API void ezgrpc2_health_set_status(const char *service, ezgrpc2_health_status status) {
  pthread_mutex_lock(&s_health_lock);
  if (service == nullptr || *service == '\0') {
    s_overall_status = status;
    pthread_mutex_unlock(&s_health_lock);
    return;
  }

  for (size_t i = 0; i < s_health_entries_count; i++) {
    if (strncmp(s_health_entries[i].name, service, MAX_SERVICE_NAME_LEN - 1) == 0) {
      s_health_entries[i].status = status;
      pthread_mutex_unlock(&s_health_lock);
      return;
    }
  }

  if (s_health_entries_count < MAX_HEALTH_SERVICES) {
    strncpy(s_health_entries[s_health_entries_count].name, service, MAX_SERVICE_NAME_LEN - 1);
    s_health_entries[s_health_entries_count].name[MAX_SERVICE_NAME_LEN - 1] = '\0';
    s_health_entries[s_health_entries_count].status = status;
    s_health_entries_count++;
  }
  pthread_mutex_unlock(&s_health_lock);
}

EZGRPC2_API ezgrpc2_health_status ezgrpc2_health_get_status(const char *service) {
  pthread_mutex_lock(&s_health_lock);
  if (service == nullptr || *service == '\0') {
    ezgrpc2_health_status res = s_overall_status;
    pthread_mutex_unlock(&s_health_lock);
    return res;
  }

  for (size_t i = 0; i < s_health_entries_count; i++) {
    if (strncmp(s_health_entries[i].name, service, MAX_SERVICE_NAME_LEN - 1) == 0) {
      ezgrpc2_health_status res = s_health_entries[i].status;
      pthread_mutex_unlock(&s_health_lock);
      return res;
    }
  }
  pthread_mutex_unlock(&s_health_lock);
  return EZGRPC2_HEALTH_SERVING; // Default to SERVING for existing services
}

EZGRPC2_API int ezgrpc2_health_register(ezgrpc2_server *server) {
  if (server == nullptr) {
    return 1;
  }
  return ezgrpc2_server_register_path(server, EZGRPC2_HEALTH_PATH_CHECK,
                                     (void *)&s_health_userdata_marker, 0, 0);
}

static void parse_health_request_service(const uint8_t *data, size_t len, char *out_service, size_t max_len) {
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
        if (!(b & 0x80)) break;
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
      while (idx < len && (data[idx++] & 0x80)) {}
    } else if (wire_type == 2) {
      // skip length-delimited
      size_t skip_len = 0;
      size_t shift = 0;
      while (idx < len) {
        uint8_t b = data[idx++];
        skip_len |= (size_t)(b & 0x7F) << shift;
        if (!(b & 0x80)) break;
        shift += 7;
      }
      idx += skip_len;
    } else {
      break;
    }
  }
}

EZGRPC2_API bool ezgrpc2_health_handle_event(ezgrpc2_server *server, ezgrpc2_event *event) {
  if (server == nullptr || event == nullptr) {
    return false;
  }
  if (event->userdata != (void *)&s_health_userdata_marker) {
    return false;
  }

  if (event->type != EZGRPC2_EVENT_MESSAGE) {
    return true;
  }

  char service_name[MAX_SERVICE_NAME_LEN] = {};
  ezgrpc2_message *in_msg = ezgrpc2_messages_peek_front(event->message.messages);
  if (in_msg != nullptr && in_msg->len > 0) {
    parse_health_request_service(in_msg->data, in_msg->len, service_name, sizeof(service_name));
  }

  ezgrpc2_health_status status = ezgrpc2_health_get_status(service_name);

  // Response: HealthCheckResponse { ServingStatus status = 1; }
  // Tag: (1 << 3) | 0 = 0x08, Value: (uint8_t)status
  uint8_t resp_buf[2] = {0x08, (uint8_t)status};
  ezgrpc2_message *out_msg = ezgrpc2_message_new(0, resp_buf, sizeof(resp_buf));
  ezgrpc2_messages *msgs = ezgrpc2_messages_new(nullptr);
  ezgrpc2_messages_push(msgs, out_msg);

  ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, msgs);
  ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_OK);
  ezgrpc2_messages_free(msgs);

  return true;
}
