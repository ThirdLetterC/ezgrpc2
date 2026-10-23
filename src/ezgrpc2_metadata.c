#include <stdlib.h>
#include <string.h>
#include <strings.h>
#include <nghttp2/nghttp2.h>
#include "ezgrpc2_metadata.h"
#include "ezgrpc2_mem.h"
#include "core.h"
#include "internal_helpers.h"

static const char b64_table[] = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+/";

char *ezgrpc2_metadata_encode_bin(const uint8_t *data, size_t in_len, size_t *out_len) {
  if (data == nullptr) {
    return nullptr;
  }

  size_t olen = 4 * ((in_len + 2) / 3);
  char *out = ezgrpc2_malloc(olen + 1);
  if (out == nullptr) {
    return nullptr;
  }

  size_t i = 0, j = 0;
  while (i < in_len) {
    size_t rem = in_len - i;
    uint32_t octet_a = (uint8_t)data[i++];
    uint32_t octet_b = (rem > 1) ? (uint8_t)data[i++] : 0;
    uint32_t octet_c = (rem > 2) ? (uint8_t)data[i++] : 0;
    uint32_t triple = (octet_a << 16) | (octet_b << 8) | octet_c;

    out[j++] = b64_table[(triple >> 18) & 0x3F];
    out[j++] = b64_table[(triple >> 12) & 0x3F];
    out[j++] = (rem > 1) ? b64_table[(triple >> 6) & 0x3F] : '=';
    out[j++] = (rem > 2) ? b64_table[triple & 0x3F] : '=';
  }
  out[j] = '\0';

  if (out_len != nullptr) {
    *out_len = j;
  }
  return out;
}

static inline int b64_char_val(char c) {
  if (c >= 'A' && c <= 'Z')
    return c - 'A';
  if (c >= 'a' && c <= 'z')
    return c - 'a' + 26;
  if (c >= '0' && c <= '9')
    return c - '0' + 52;
  if (c == '+')
    return 62;
  if (c == '/')
    return 63;
  return -1;
}

uint8_t *ezgrpc2_metadata_decode_bin(const char *base64_str, size_t in_len, size_t *out_len) {
  if (base64_str == nullptr) {
    return nullptr;
  }

  // Count valid base64 characters (strip trailing '=')
  while (in_len > 0 && (base64_str[in_len - 1] == '=' || base64_str[in_len - 1] == ' ' ||
                        base64_str[in_len - 1] == '\n')) {
    in_len--;
  }

  if (in_len == 0) {
    uint8_t *empty = ezgrpc2_malloc(1);
    if (out_len)
      *out_len = 0;
    return empty;
  }

  size_t max_out = (in_len * 3) / 4 + 4;
  uint8_t *out = ezgrpc2_malloc(max_out);
  if (out == nullptr) {
    return nullptr;
  }

  size_t i = 0, j = 0;
  uint32_t buf = 0;
  int bits = 0;

  while (i < in_len) {
    int val = b64_char_val(base64_str[i++]);
    if (val < 0)
      continue;
    buf = (buf << 6) | (uint32_t)val;
    bits += 6;
    if (bits >= 8) {
      bits -= 8;
      out[j++] = (uint8_t)((buf >> bits) & 0xFF);
    }
  }

  if (out_len != nullptr) {
    *out_len = j;
  }
  return out;
}

static ezgrpc2_stream *find_stream(ezgrpc2_server *server, ezgrpc2_session_uuid session_uuid,
                                   int32_t stream_id) {
  if (server == nullptr)
    return nullptr;
  ezgrpc2_session *session = session_find(server->sessions, server->nb_sessions, session_uuid);
  if (session == nullptr || session->ngsession == nullptr)
    return nullptr;
  return (ezgrpc2_stream *)nghttp2_session_get_stream_user_data(session->ngsession, stream_id);
}

EZGRPC2_API const char *ezgrpc2_metadata_get(ezgrpc2_server *server,
                                             ezgrpc2_session_uuid session_uuid, int32_t stream_id,
                                             const char *key, size_t *value_len) {
  if (key == nullptr)
    return nullptr;
  ezgrpc2_stream *stream = find_stream(server, session_uuid, stream_id);
  if (stream == nullptr)
    return nullptr;

  size_t klen = strlen(key);
  for (size_t i = 0; i < stream->nb_headers; i++) {
    if (stream->headers[i].namelen == klen &&
        strncasecmp(stream->headers[i].name, key, klen) == 0) {
      if (value_len != nullptr) {
        *value_len = stream->headers[i].valuelen;
      }
      return stream->headers[i].value;
    }
  }
  return nullptr;
}

EZGRPC2_API size_t ezgrpc2_metadata_count(ezgrpc2_server *server, ezgrpc2_session_uuid session_uuid,
                                          int32_t stream_id) {
  ezgrpc2_stream *stream = find_stream(server, session_uuid, stream_id);
  return stream ? stream->nb_headers : 0;
}

EZGRPC2_API int ezgrpc2_metadata_get_at(ezgrpc2_server *server, ezgrpc2_session_uuid session_uuid,
                                        int32_t stream_id, size_t idx,
                                        ezgrpc2_metadata_entry *out_entry) {
  if (out_entry == nullptr)
    return 1;
  ezgrpc2_stream *stream = find_stream(server, session_uuid, stream_id);
  if (stream == nullptr || idx >= stream->nb_headers)
    return 1;

  out_entry->key = stream->headers[idx].name;
  out_entry->key_len = stream->headers[idx].namelen;
  out_entry->value = stream->headers[idx].value;
  out_entry->value_len = stream->headers[idx].valuelen;
  return 0;
}

EZGRPC2_API int ezgrpc2_server_session_stream_send_initial_metadata(
    ezgrpc2_server *server, ezgrpc2_session_uuid session_uuid, int32_t stream_id,
    const ezgrpc2_metadata_entry *entries, size_t count) {
  if (server == nullptr || entries == nullptr || count == 0)
    return 1;
  ezgrpc2_session *session = session_find(server->sessions, server->nb_sessions, session_uuid);
  if (session == nullptr || session->ngsession == nullptr)
    return 1;

  nghttp2_nv *nva = ezgrpc2_malloc(count * sizeof(nghttp2_nv));
  if (nva == nullptr)
    return 1;

  for (size_t i = 0; i < count; i++) {
    nva[i] = (nghttp2_nv){
        .name = (uint8_t *)entries[i].key,
        .value = (uint8_t *)entries[i].value,
        .namelen = entries[i].key_len ? entries[i].key_len : strlen(entries[i].key),
        .valuelen = entries[i].value_len ? entries[i].value_len : strlen(entries[i].value),
        .flags = NGHTTP2_NV_FLAG_NONE,
    };
  }

  int res = nghttp2_submit_headers(session->ngsession, 0, stream_id, nullptr, nva, count, nullptr);
  ezgrpc2_free(nva);
  if (res != 0)
    return 1;

  session_schedule_flush(session);
  return 0;
}
