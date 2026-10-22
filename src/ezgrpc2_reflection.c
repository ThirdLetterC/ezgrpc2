#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <pthread.h>
#include "ezgrpc2_reflection.h"
#include "ezgrpc2_session.h"
#include "ezgrpc2_message.h"
#include "ezgrpc2_messages.h"
#include "ezgrpc2_grpc_status.h"
#include "ezgrpc2_mem.h"
#include "core.h"

#define MAX_DESCRIPTORS 32
#define MAX_SYMBOL_LEN 128

typedef struct descriptor_entry {
  char symbol[MAX_SYMBOL_LEN];
  uint8_t *data;
  size_t len;
} descriptor_entry;

static descriptor_entry s_descriptors[MAX_DESCRIPTORS];
static size_t s_descriptors_count = 0;
static pthread_mutex_t s_reflection_lock = PTHREAD_MUTEX_INITIALIZER;

static const int s_reflection_userdata_marker = 0x5245464C; // "REFL"

static size_t varint_size(uint64_t val) {
  size_t size = 0;
  do {
    size++;
    val >>= 7;
  } while (val != 0);
  return size;
}

static size_t write_varint(uint8_t *buf, uint64_t val) {
  size_t len = 0;
  do {
    uint8_t b = (uint8_t)(val & 0x7F);
    val >>= 7;
    if (val != 0) b |= 0x80;
    buf[len++] = b;
  } while (val != 0);
  return len;
}

static uint64_t read_varint(const uint8_t *buf, size_t len, size_t *seek) {
  uint64_t val = 0;
  size_t shift = 0;
  while (*seek < len) {
    uint8_t b = buf[(*seek)++];
    val |= (uint64_t)(b & 0x7F) << shift;
    if (!(b & 0x80)) break;
    shift += 7;
  }
  return val;
}

EZGRPC2_API int ezgrpc2_reflection_add_file_descriptor(
  const char *symbol_or_filename,
  const uint8_t *descriptor_bytes,
  size_t descriptor_len) {
  if (symbol_or_filename == nullptr || descriptor_bytes == nullptr || descriptor_len == 0) {
    return 1;
  }

  pthread_mutex_lock(&s_reflection_lock);
  if (s_descriptors_count >= MAX_DESCRIPTORS) {
    pthread_mutex_unlock(&s_reflection_lock);
    return 2;
  }

  uint8_t *copy = ezgrpc2_malloc(descriptor_len);
  if (copy == nullptr) {
    pthread_mutex_unlock(&s_reflection_lock);
    return 3;
  }
  memcpy(copy, descriptor_bytes, descriptor_len);

  descriptor_entry *e = &s_descriptors[s_descriptors_count++];
  strncpy(e->symbol, symbol_or_filename, sizeof(e->symbol) - 1);
  e->symbol[sizeof(e->symbol) - 1] = '\0';
  e->data = copy;
  e->len = descriptor_len;

  pthread_mutex_unlock(&s_reflection_lock);
  return 0;
}

EZGRPC2_API int ezgrpc2_reflection_register(ezgrpc2_server *server) {
  if (server == nullptr) {
    return 1;
  }
  int rc1 = ezgrpc2_server_register_path(server, EZGRPC2_REFLECTION_V1ALPHA_PATH,
                                       (void *)&s_reflection_userdata_marker, 0, 0);
  int rc2 = ezgrpc2_server_register_path(server, EZGRPC2_REFLECTION_V1_PATH,
                                       (void *)&s_reflection_userdata_marker, 0, 0);
  return (rc1 == 0 && rc2 == 0) ? 0 : 1;
}

static size_t collect_services(ezgrpc2_server *server, char services[][MAX_SYMBOL_LEN], size_t max_services) {
  size_t count = 0;

  for (size_t i = 0; i < server->nb_paths && count < max_services; i++) {
    const char *p = server->paths[i].path;
    if (p == nullptr || *p != '/') continue;

    const char *service_start = p + 1;
    const char *service_end = strchr(service_start, '/');
    if (service_end == nullptr) continue;

    size_t s_len = (size_t)(service_end - service_start);
    if (s_len == 0 || s_len >= MAX_SYMBOL_LEN) continue;

    char svc[MAX_SYMBOL_LEN];
    memcpy(svc, service_start, s_len);
    svc[s_len] = '\0';

    // Check if already in list
    bool dup = false;
    for (size_t k = 0; k < count; k++) {
      if (strcmp(services[k], svc) == 0) {
        dup = true;
        break;
      }
    }
    if (!dup) {
      strcpy(services[count++], svc);
    }
  }

  // Always ensure reflection service itself is advertised
  const char *refl_svc = "grpc.reflection.v1alpha.ServerReflection";
  bool has_refl = false;
  for (size_t k = 0; k < count; k++) {
    if (strcmp(services[k], refl_svc) == 0) {
      has_refl = true;
      break;
    }
  }
  if (!has_refl && count < max_services) {
    strcpy(services[count++], refl_svc);
  }

  return count;
}

static uint8_t *build_list_services_response(
  const char services[][MAX_SYMBOL_LEN],
  size_t nb_services,
  const uint8_t *orig_req,
  size_t orig_req_len,
  size_t *out_resp_len) {

  // Calculate size of ListServiceResponse
  size_t list_resp_inner = 0;
  for (size_t i = 0; i < nb_services; i++) {
    size_t nlen = strlen(services[i]);
    // ServiceResponse: field 1 (string name), tag 0x0A, varint(nlen), nlen
    size_t s_size = 1 + varint_size(nlen) + nlen;
    // ListServiceResponse.service: tag 0x0A, varint(s_size), s_size
    list_resp_inner += 1 + varint_size(s_size) + s_size;
  }

  // ServerReflectionResponse:
  // Field 2: original_request (tag 0x12, varint orig_req_len, orig_req)
  size_t orig_req_field_size = 0;
  if (orig_req_len > 0) {
    orig_req_field_size = 1 + varint_size(orig_req_len) + orig_req_len;
  }
  // Field 6: list_services_response (tag (6 << 3) | 2 = 0x32, varint list_resp_inner, inner)
  size_t list_field_size = 1 + varint_size(list_resp_inner) + list_resp_inner;

  size_t total = orig_req_field_size + list_field_size;
  uint8_t *buf = ezgrpc2_malloc(total);
  if (buf == nullptr) return nullptr;

  size_t off = 0;
  if (orig_req_len > 0) {
    buf[off++] = (2 << 3) | 2; // 0x12
    off += write_varint(buf + off, orig_req_len);
    memcpy(buf + off, orig_req, orig_req_len);
    off += orig_req_len;
  }

  buf[off++] = (6 << 3) | 2; // 0x32
  off += write_varint(buf + off, list_resp_inner);

  for (size_t i = 0; i < nb_services; i++) {
    size_t nlen = strlen(services[i]);
    size_t s_size = 1 + varint_size(nlen) + nlen;

    buf[off++] = (1 << 3) | 2; // 0x0A
    off += write_varint(buf + off, s_size);

    buf[off++] = (1 << 3) | 2; // 0x0A
    off += write_varint(buf + off, nlen);
    memcpy(buf + off, services[i], nlen);
    off += nlen;
  }

  *out_resp_len = off;
  return buf;
}

EZGRPC2_API bool ezgrpc2_reflection_handle_event(
  ezgrpc2_server *server,
  ezgrpc2_event *event) {
  if (server == nullptr || event == nullptr) {
    return false;
  }
  if (event->userdata != (void *)&s_reflection_userdata_marker) {
    return false;
  }

  if (event->type != EZGRPC2_EVENT_MESSAGE) {
    return true;
  }

  ezgrpc2_message *in_msg = ezgrpc2_messages_peek_front(event->message.messages);
  if (in_msg == nullptr) {
    if (event->message.end_stream) {
      ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_OK);
    }
    return true;
  }

  // Parse request
  const uint8_t *req_data = in_msg->data;
  size_t req_len = in_msg->len;

  bool is_list_services = false;
  char queried_symbol[MAX_SYMBOL_LEN] = {};
  bool is_file_query = false;

  size_t seek = 0;
  while (seek < req_len) {
    uint8_t tag = req_data[seek++];
    uint8_t field_num = tag >> 3;
    uint8_t wire_type = tag & 0x07;

    if (field_num == 7 && wire_type == 2) {
      // list_services
      size_t slen = (size_t)read_varint(req_data, req_len, &seek);
      seek += slen;
      is_list_services = true;
    } else if ((field_num == 3 || field_num == 4) && wire_type == 2) {
      // file_by_filename (3) or file_containing_symbol (4)
      size_t slen = (size_t)read_varint(req_data, req_len, &seek);
      if (seek + slen <= req_len) {
        size_t cpy = slen < MAX_SYMBOL_LEN - 1 ? slen : MAX_SYMBOL_LEN - 1;
        memcpy(queried_symbol, req_data + seek, cpy);
        queried_symbol[cpy] = '\0';
      }
      seek += slen;
      is_file_query = true;
    } else if (wire_type == 0) {
      read_varint(req_data, req_len, &seek);
    } else if (wire_type == 2) {
      size_t skip = (size_t)read_varint(req_data, req_len, &seek);
      seek += skip;
    } else {
      break;
    }
  }

  uint8_t *resp_bytes = nullptr;
  size_t resp_len = 0;

  if (is_list_services || (!is_file_query && req_len <= 2)) {
    // List services request
    char services[64][MAX_SYMBOL_LEN];
    size_t nb_services = collect_services(server, services, 64);
    resp_bytes = build_list_services_response(services, nb_services, req_data, req_len, &resp_len);
  } else if (is_file_query) {
    pthread_mutex_lock(&s_reflection_lock);
    const descriptor_entry *found = nullptr;
    for (size_t i = 0; i < s_descriptors_count; i++) {
      if (strstr(s_descriptors[i].symbol, queried_symbol) || strstr(queried_symbol, s_descriptors[i].symbol)) {
        found = &s_descriptors[i];
        break;
      }
    }

    if (found != nullptr) {
      // FileDescriptorResponse: field 1 (repeated bytes file_descriptor_proto = 1)
      size_t f_size = 1 + varint_size(found->len) + found->len;
      // ServerReflectionResponse.file_descriptor_response: tag 0x22, varint(f_size)
      size_t orig_req_size = req_len > 0 ? 1 + varint_size(req_len) + req_len : 0;
      size_t total = orig_req_size + 1 + varint_size(f_size) + f_size;
      resp_bytes = ezgrpc2_malloc(total);
      if (resp_bytes != nullptr) {
        size_t off = 0;
        if (req_len > 0) {
          resp_bytes[off++] = 0x12;
          off += write_varint(resp_bytes + off, req_len);
          memcpy(resp_bytes + off, req_data, req_len);
          off += req_len;
        }
        resp_bytes[off++] = (4 << 3) | 2; // 0x22
        off += write_varint(resp_bytes + off, f_size);

        resp_bytes[off++] = (1 << 3) | 2; // 0x0A
        off += write_varint(resp_bytes + off, found->len);
        memcpy(resp_bytes + off, found->data, found->len);
        off += found->len;
        resp_len = off;
      }
    } else {
      // ErrorResponse: error_code = 5 (NOT_FOUND), error_message = "Not found"
      const char *err_msg = "Symbol or file not found";
      size_t elen = strlen(err_msg);
      size_t err_inner = 1 + varint_size(5) + 1 + varint_size(elen) + elen;
      size_t orig_req_size = req_len > 0 ? 1 + varint_size(req_len) + req_len : 0;
      size_t total = orig_req_size + 1 + varint_size(err_inner) + err_inner;

      resp_bytes = ezgrpc2_malloc(total);
      if (resp_bytes != nullptr) {
        size_t off = 0;
        if (req_len > 0) {
          resp_bytes[off++] = 0x12;
          off += write_varint(resp_bytes + off, req_len);
          memcpy(resp_bytes + off, req_data, req_len);
          off += req_len;
        }
        resp_bytes[off++] = (7 << 3) | 2; // 0x3A (error_response)
        off += write_varint(resp_bytes + off, err_inner);

        resp_bytes[off++] = (1 << 3) | 0; // 0x08 (error_code)
        off += write_varint(resp_bytes + off, 5); // NOT_FOUND

        resp_bytes[off++] = (2 << 3) | 2; // 0x12 (error_message)
        off += write_varint(resp_bytes + off, elen);
        memcpy(resp_bytes + off, err_msg, elen);
        off += elen;
        resp_len = off;
      }
    }
    pthread_mutex_unlock(&s_reflection_lock);
  }

  if (resp_bytes != nullptr) {
    ezgrpc2_message *out_msg = ezgrpc2_message_new(0, resp_bytes, resp_len);
    ezgrpc2_free(resp_bytes);
    ezgrpc2_messages *res_msgs = ezgrpc2_messages_new(nullptr);
    ezgrpc2_messages_push(res_msgs, out_msg);

    ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, res_msgs);
    ezgrpc2_messages_free(res_msgs);
  }

  if (event->message.end_stream) {
    ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_OK);
  }

  return true;
}
