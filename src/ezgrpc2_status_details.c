#include <stdlib.h>
#include <string.h>
#include "ezgrpc2_status_details.h"
#include "ezgrpc2_metadata.h"
#include "ezgrpc2_session.h"
#include "ezgrpc2_mem.h"

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

EZGRPC2_API char *ezgrpc2_status_details_encode(
  int32_t code,
  const char *message,
  const ezgrpc2_status_detail_entry *details,
  size_t nb_details,
  size_t *out_len) {
  size_t msg_len = message ? strlen(message) : 0;

  // Calculate total protobuf byte size
  // Field 1: code (tag 1, wire 0)
  size_t total_size = 1 + varint_size((uint32_t)code);

  // Field 2: message (tag 2, wire 2)
  if (msg_len > 0) {
    total_size += 1 + varint_size(msg_len) + msg_len;
  }

  // Field 3: details (repeated Any)
  for (size_t i = 0; i < nb_details; i++) {
    size_t url_len = details[i].type_url ? strlen(details[i].type_url) : 0;
    size_t val_len = details[i].value_len;

    size_t any_inner_size = 0;
    if (url_len > 0) {
      any_inner_size += 1 + varint_size(url_len) + url_len;
    }
    if (val_len > 0) {
      any_inner_size += 1 + varint_size(val_len) + val_len;
    }

    total_size += 1 + varint_size(any_inner_size) + any_inner_size;
  }

  uint8_t *pb_buf = ezgrpc2_malloc(total_size);
  if (pb_buf == nullptr) {
    return nullptr;
  }

  size_t offset = 0;

  // 1. Write Field 1: code
  pb_buf[offset++] = (1 << 3) | 0; // 0x08
  offset += write_varint(pb_buf + offset, (uint32_t)code);

  // 2. Write Field 2: message
  if (msg_len > 0) {
    pb_buf[offset++] = (2 << 3) | 2; // 0x12
    offset += write_varint(pb_buf + offset, msg_len);
    memcpy(pb_buf + offset, message, msg_len);
    offset += msg_len;
  }

  // 3. Write Field 3: details
  for (size_t i = 0; i < nb_details; i++) {
    size_t url_len = details[i].type_url ? strlen(details[i].type_url) : 0;
    size_t val_len = details[i].value_len;

    size_t any_inner_size = 0;
    if (url_len > 0) {
      any_inner_size += 1 + varint_size(url_len) + url_len;
    }
    if (val_len > 0) {
      any_inner_size += 1 + varint_size(val_len) + val_len;
    }

    // Status.details tag (3, wire 2)
    pb_buf[offset++] = (3 << 3) | 2; // 0x1A
    offset += write_varint(pb_buf + offset, any_inner_size);

    // Any.type_url (field 1, wire 2)
    if (url_len > 0) {
      pb_buf[offset++] = (1 << 3) | 2; // 0x0A
      offset += write_varint(pb_buf + offset, url_len);
      memcpy(pb_buf + offset, details[i].type_url, url_len);
      offset += url_len;
    }

    // Any.value (field 2, wire 2)
    if (val_len > 0) {
      pb_buf[offset++] = (2 << 3) | 2; // 0x12
      offset += write_varint(pb_buf + offset, val_len);
      memcpy(pb_buf + offset, details[i].value, val_len);
      offset += val_len;
    }
  }

  // Base64 encode for trailing metadata
  char *b64 = ezgrpc2_metadata_encode_bin(pb_buf, offset, out_len);
  ezgrpc2_free(pb_buf);
  return b64;
}

EZGRPC2_API int ezgrpc2_status_details_decode(
  const char *base64_str,
  int32_t *out_code,
  char **out_message,
  char **out_type_url,
  uint8_t **out_detail_value,
  size_t *out_detail_len) {
  if (base64_str == nullptr) {
    return 1;
  }

  size_t raw_len = 0;
  uint8_t *raw = ezgrpc2_metadata_decode_bin(base64_str, strlen(base64_str), &raw_len);
  if (raw == nullptr) {
    return 2;
  }

  if (out_code) *out_code = 0;
  if (out_message) *out_message = nullptr;
  if (out_type_url) *out_type_url = nullptr;
  if (out_detail_value) *out_detail_value = nullptr;
  if (out_detail_len) *out_detail_len = 0;

  size_t seek = 0;
  while (seek < raw_len) {
    uint8_t tag = raw[seek++];
    uint8_t field_num = tag >> 3;
    uint8_t wire_type = tag & 0x07;

    if (field_num == 1 && wire_type == 0) {
      // code
      uint64_t c = read_varint(raw, raw_len, &seek);
      if (out_code) *out_code = (int32_t)c;
    } else if (field_num == 2 && wire_type == 2) {
      // message
      size_t mlen = (size_t)read_varint(raw, raw_len, &seek);
      if (seek + mlen <= raw_len && out_message) {
        char *m = ezgrpc2_malloc(mlen + 1);
        if (m) {
          memcpy(m, raw + seek, mlen);
          m[mlen] = '\0';
          *out_message = m;
        }
      }
      seek += mlen;
    } else if (field_num == 3 && wire_type == 2) {
      // Any
      size_t any_len = (size_t)read_varint(raw, raw_len, &seek);
      size_t any_end = seek + any_len;
      while (seek < any_end && seek < raw_len) {
        uint8_t any_tag = raw[seek++];
        uint8_t any_field = any_tag >> 3;
        uint8_t any_wire = any_tag & 0x07;

        if (any_field == 1 && any_wire == 2) {
          size_t ulen = (size_t)read_varint(raw, raw_len, &seek);
          if (seek + ulen <= any_end && out_type_url) {
            char *u = ezgrpc2_malloc(ulen + 1);
            if (u) {
              memcpy(u, raw + seek, ulen);
              u[ulen] = '\0';
              *out_type_url = u;
            }
          }
          seek += ulen;
        } else if (any_field == 2 && any_wire == 2) {
          size_t vlen = (size_t)read_varint(raw, raw_len, &seek);
          if (seek + vlen <= any_end && out_detail_value) {
            uint8_t *v = ezgrpc2_malloc(vlen);
            if (v) {
              memcpy(v, raw + seek, vlen);
              *out_detail_value = v;
              if (out_detail_len) *out_detail_len = vlen;
            }
          }
          seek += vlen;
        } else if (any_wire == 0) {
          read_varint(raw, raw_len, &seek);
        } else if (any_wire == 2) {
          size_t sk = (size_t)read_varint(raw, raw_len, &seek);
          seek += sk;
        }
      }
    } else if (wire_type == 0) {
      read_varint(raw, raw_len, &seek);
    } else if (wire_type == 2) {
      size_t sk = (size_t)read_varint(raw, raw_len, &seek);
      seek += sk;
    }
  }

  ezgrpc2_free(raw);
  return 0;
}

EZGRPC2_API int ezgrpc2_server_session_stream_end_with_status_details(
  ezgrpc2_server *server,
  ezgrpc2_session_uuid session_uuid,
  int32_t stream_id,
  ezgrpc2_grpc_status status,
  const char *message,
  const ezgrpc2_status_detail_entry *details,
  size_t nb_details,
  const ezgrpc2_metadata_entry *extra_trailers,
  size_t nb_extra_trailers) {
  size_t b64_len = 0;
  char *b64_status = ezgrpc2_status_details_encode(
    (int32_t)status, message, details, nb_details, &b64_len);

  size_t total_trailers = nb_extra_trailers + (b64_status ? 1 : 0);
  ezgrpc2_metadata_entry *all_trailers = nullptr;
  if (total_trailers > 0) {
    all_trailers = ezgrpc2_malloc(total_trailers * sizeof(*all_trailers));
    if (all_trailers != nullptr) {
      for (size_t i = 0; i < nb_extra_trailers; i++) {
        all_trailers[i] = extra_trailers[i];
      }
      if (b64_status != nullptr) {
        all_trailers[nb_extra_trailers] = (ezgrpc2_metadata_entry){
          .key = EZGRPC2_STATUS_DETAILS_TRAILER,
          .key_len = strlen(EZGRPC2_STATUS_DETAILS_TRAILER),
          .value = b64_status,
          .value_len = b64_len,
        };
      }
    }
  }

  int res = ezgrpc2_server_session_stream_end2(
    server, session_uuid, stream_id, status, message,
    all_trailers, total_trailers);

  if (all_trailers != nullptr) {
    ezgrpc2_free(all_trailers);
  }
  if (b64_status != nullptr) {
    ezgrpc2_free(b64_status);
  }
  return res;
}
