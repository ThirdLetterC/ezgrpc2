/* ezgrpc2_call.c - calls: their lifetime, dispatch and the call API
 *
 * Copyright (c) 2023-2025 M. N. Yoshie & Al-buharie Amjari
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions
 * are met:
 *
 *   1. Redistributions of source code must retain the above copyright notice,
 *   this list of conditions and the following disclaimer.
 *
 *   2. Neither the name of the copyright holder nor the names of its
 *   contributors may be used to endorse or promote products derived from
 *   this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS
 * “AS IS” AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT
 * LIMITED TO, THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR
 * A PARTICULAR PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL THE COPYRIGHT
 * HOLDER OR CONTRIBUTORS BE LIABLE FOR ANY DIRECT, INDIRECT, INCIDENTAL,
 * SPECIAL, EXEMPLARY, OR CONSEQUENTIAL DAMAGES (INCLUDING, BUT NOT LIMITED
 * TO, PROCUREMENT OF SUBSTITUTE GOODS OR SERVICES; LOSS OF USE, DATA, OR
 * PROFITS; OR BUSINESS INTERRUPTION) HOWEVER CAUSED AND ON ANY THEORY OF
 * LIABILITY, WHETHER IN CONTRACT, STRICT LIABILITY, OR TORT (INCLUDING
 * NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY OUT OF THE USE OF THIS
 * SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH DAMAGE.
 */


#include <stdarg.h>
#include <stdio.h>
#include <strings.h>

#include "core.h"
#include "internal_helpers.h"
#include "internal_nghttp2_callbacks.h"
#include "ezgrpc2_timeout.h"

/* ---- header lists ------------------------------------------------------------ */

static ezgrpc2_err nv_list_add(nv_list *list, const char *name, size_t namelen, const char *value,
                               size_t valuelen) {
  if (list->len == list->cap) {
    size_t cap = list->cap ? list->cap * 2 : 4;
    nghttp2_nv *nva = ezgrpc2_realloc(list->nva, cap * sizeof(*nva));
    if (nva == nullptr)
      return EZGRPC2_ERR_NOMEM;
    list->nva = nva;
    list->cap = cap;
  }
  /* name and value share one allocation, freed through the name */
  u8 *block = ezgrpc2_malloc(namelen + valuelen + 2);
  if (block == nullptr)
    return EZGRPC2_ERR_NOMEM;
  memcpy(block, name, namelen);
  block[namelen] = '\0';
  memcpy(block + namelen + 1, value, valuelen);
  block[namelen + 1 + valuelen] = '\0';
  list->nva[list->len++] = (nghttp2_nv){
      .name = block,
      .namelen = namelen,
      .value = block + namelen + 1,
      .valuelen = valuelen,
      .flags = NGHTTP2_NV_FLAG_NONE,
  };
  return EZGRPC2_OK;
}

static void nv_list_free(nv_list *list) {
  for (size_t i = 0; i < list->len; i++)
    ezgrpc2_free(list->nva[i].name);
  ezgrpc2_free(list->nva);
  *list = (nv_list){};
}

static ezgrpc2_err nv_list_add_metadata(nv_list *list, const ezgrpc2_metadata *metadata,
                                        size_t count) {
  if (metadata == nullptr && count > 0)
    return EZGRPC2_ERR_INVALID;
  for (size_t i = 0; i < count; i++) {
    const ezgrpc2_metadata *md = &metadata[i];
    if (md->key == nullptr || md->value == nullptr)
      return EZGRPC2_ERR_INVALID;
    size_t klen = md->key_len ? md->key_len : strlen(md->key);
    size_t vlen = md->value_len ? md->value_len : strlen(md->value);
    /* pseudo-headers belong to HTTP/2 itself */
    if (klen == 0 || md->key[0] == ':')
      return EZGRPC2_ERR_INVALID;
    ezgrpc2_err err = nv_list_add(list, md->key, klen, md->value, vlen);
    if (err != EZGRPC2_OK)
      return err;
  }
  return EZGRPC2_OK;
}

/* copies nva into a single allocation */
static nghttp2_nv *nva_dup(const nghttp2_nv *nva, size_t nvlen) {
  size_t size = nvlen * sizeof(*nva);
  for (size_t i = 0; i < nvlen; i++)
    size += nva[i].namelen + nva[i].valuelen;
  nghttp2_nv *copy = ezgrpc2_malloc(size);
  if (copy == nullptr)
    return nullptr;
  u8 *p = (u8 *)(copy + nvlen);
  for (size_t i = 0; i < nvlen; i++) {
    copy[i] = nva[i];
    copy[i].name = p;
    memcpy(p, nva[i].name, nva[i].namelen);
    p += nva[i].namelen;
    copy[i].value = p;
    memcpy(p, nva[i].value, nva[i].valuelen);
    p += nva[i].valuelen;
  }
  return copy;
}

/* ---- lifetime ------------------------------------------------------------------ */

static void drop_pending(ezgrpc2_call *call) {
  ezgrpc2_message *msg;
  while ((msg = ezgrpc2_list_pop_front(&call->pending)) != nullptr)
    ezgrpc2_message_free(msg);
}

static void call_free(ezgrpc2_call *call) {
  drop_pending(call);
  ezgrpc2_message_free(call->request);
  nv_list_free(&call->headers);
  nv_list_free(&call->trailers);
  ezgrpc2_free(call->req_headers);
  if (call->userdata_free != nullptr)
    call->userdata_free(call->userdata);
  ezgrpc2_free(call);
}

/* copies the request headers, which nghttp2 frees with the stream */
static bool copy_request_headers(ezgrpc2_call *call, const ezgrpc2_stream *stream) {
  size_t size = stream->nb_headers * sizeof(ezgrpc2_header);
  for (size_t i = 0; i < stream->nb_headers; i++)
    size += stream->headers[i].namelen + stream->headers[i].valuelen + 2;
  ezgrpc2_header *headers = ezgrpc2_malloc(size ? size : 1);
  if (headers == nullptr)
    return false;
  char *p = (char *)(headers + stream->nb_headers);
  for (size_t i = 0; i < stream->nb_headers; i++) {
    const ezgrpc2_header *h = &stream->headers[i];
    memcpy(p, h->name, h->namelen + 1);
    headers[i].name = p;
    headers[i].namelen = h->namelen;
    p += h->namelen + 1;
    memcpy(p, h->value, h->valuelen + 1);
    headers[i].value = p;
    headers[i].valuelen = h->valuelen;
    p += h->valuelen + 1;
  }
  call->req_headers = headers;
  call->nb_req_headers = stream->nb_headers;
  return true;
}

ezgrpc2_call *call_new(ezgrpc2_session *session, ezgrpc2_stream *stream) {
  ezgrpc2_call *call = ezgrpc2_calloc(1, sizeof(*call));
  if (call == nullptr)
    return nullptr;
  if (!copy_request_headers(call, stream)) {
    ezgrpc2_free(call);
    return nullptr;
  }
  /* the stream's reference, dropped by call_detach() */
  atomic_init(&call->refs, 1);
  call->server = session->server;
  call->route = stream->route;
  call->session = session;
  call->stream = stream;
  call->stream_id = stream->stream_id;
  call->start_ms = ezgrpc2_time_now_ms();
  memcpy(call->peer, session->peer, sizeof(call->peer));
  ezgrpc2_list_init(&call->pending);
  return call;
}

EZGRPC2_API ezgrpc2_call *ezgrpc2_call_ref(ezgrpc2_call *call) {
  if (call != nullptr)
    atomic_fetch_add_explicit(&call->refs, 1, memory_order_relaxed);
  return call;
}

EZGRPC2_API void ezgrpc2_call_unref(ezgrpc2_call *call) {
  if (call != nullptr && atomic_fetch_sub_explicit(&call->refs, 1, memory_order_acq_rel) == 1)
    call_free(call);
}

void call_queue(ezgrpc2_call *call) {
  if (call->queued)
    return;
  call->queued = true;
  ezgrpc2_call_ref(call);
  ezgrpc2_server *server = call->server;
  call->ready_next = nullptr;
  if (server->ready_tail != nullptr)
    server->ready_tail->ready_next = call;
  else
    server->ready_head = call;
  server->ready_tail = call;
}

static void call_cancel(ezgrpc2_call *call, ezgrpc2_status reason) {
  if (call->cancelled)
    return;
  call->cancelled = true;
  call->cancel_reason = reason;
  interceptors_run_on_done(call, reason);
  call_queue(call);
}

void call_detach(ezgrpc2_call *call, ezgrpc2_status reason) {
  if (call->closed)
    return;
  call->closed = true;
  call->stream = nullptr;
  call->session = nullptr;
  if (!call->finished)
    call_cancel(call, reason);
  ezgrpc2_call_unref(call);
}

/* ---- responses ---------------------------------------------------------------- */

/* percent-encodes a grpc-message, as the gRPC HTTP/2 protocol requires */
static char *encode_grpc_message(const char *message, size_t *len) {
  size_t n = 0;
  for (const u8 *p = (const u8 *)message; *p; p++)
    n += (*p < 0x20 || *p > 0x7e || *p == '%') ? 3 : 1;
  char *out = ezgrpc2_malloc(n + 1);
  if (out == nullptr)
    return nullptr;
  static const char hex[] = "0123456789ABCDEF";
  char *o = out;
  for (const u8 *p = (const u8 *)message; *p; p++) {
    if (*p < 0x20 || *p > 0x7e || *p == '%') {
      *o++ = '%';
      *o++ = hex[*p >> 4];
      *o++ = hex[*p & 0xf];
    } else {
      *o++ = (char)*p;
    }
  }
  *o = '\0';
  *len = n;
  return out;
}

/* submits the response HEADERS: the fixed headers, the application's
 * metadata and `extra` (the trailers of a Trailers-Only response) */
/* String literals keep the nghttp2 name and value lengths at compile time. */
#define NGHTTP2_NV(name, value)                                                                    \
  ((nghttp2_nv){(uint8_t *)(name), (uint8_t *)(value), sizeof(name) - 1, sizeof(value) - 1,       \
                NGHTTP2_NV_FLAG_NONE})

static ezgrpc2_err submit_response_headers(ezgrpc2_call *call, uint8_t flags,
                                           const nghttp2_nv *extra, size_t nb_extra) {
#if defined(EZGRPC2_ENABLE_ZLIB)
#define ACCEPT_ZLIB "gzip,deflate,"
#else
#define ACCEPT_ZLIB ""
#endif
#if defined(EZGRPC2_ENABLE_ZSTD) && defined(EZGRPC2_ENABLE_SNAPPY)
  const char *accepted = ACCEPT_ZLIB "zstd,snappy,identity";
#elif defined(EZGRPC2_ENABLE_ZSTD)
  const char *accepted = ACCEPT_ZLIB "zstd,identity";
#elif defined(EZGRPC2_ENABLE_SNAPPY)
  const char *accepted = ACCEPT_ZLIB "snappy,identity";
#else
  const char *accepted = ACCEPT_ZLIB "identity";
#endif
#undef ACCEPT_ZLIB
  nghttp2_nv base[] = {
      NGHTTP2_NV(":status", "200"),
      NGHTTP2_NV("content-type", "application/grpc"),
      { (uint8_t *)"grpc-accept-encoding", (uint8_t *)accepted, sizeof("grpc-accept-encoding") - 1, 0, NGHTTP2_NV_FLAG_NONE },
      NGHTTP2_NV("grpc-encoding", "gzip"),
  };
  const char *encoding = "gzip";
  switch (call->compression) {
  case EZGRPC2_COMPRESS_DEFLATE: encoding = "deflate"; break;
  case EZGRPC2_COMPRESS_ZSTD: encoding = "zstd"; break;
  case EZGRPC2_COMPRESS_SNAPPY: encoding = "snappy"; break;
  default: break;
  }
  base[2].valuelen = strlen(accepted);
  base[3].value = (uint8_t *)encoding;
  base[3].valuelen = strlen(encoding);
  size_t nb_base = call->compression != EZGRPC2_COMPRESS_IDENTITY ? 4 : 3;
  size_t total = nb_base + call->headers.len + nb_extra;
  nghttp2_nv *nva = ezgrpc2_malloc(total * sizeof(*nva));
  if (nva == nullptr)
    return EZGRPC2_ERR_NOMEM;
  memcpy(nva, base, nb_base * sizeof(*nva));
  if (call->headers.len > 0)
    memcpy(nva + nb_base, call->headers.nva, call->headers.len * sizeof(*nva));
  if (nb_extra > 0)
    memcpy(nva + nb_base + call->headers.len, extra, nb_extra * sizeof(*nva));

  int res = nghttp2_submit_headers(call->session->ngsession, flags, call->stream_id, nullptr, nva,
                                   total, nullptr);
  ezgrpc2_free(nva);
  if (res != 0)
    return EZGRPC2_ERR_PROTOCOL;
  call->headers_sent = true;
  nv_list_free(&call->headers);
  return EZGRPC2_OK;
}

#undef NGHTTP2_NV

static ezgrpc2_err call_finish_internal(ezgrpc2_call *call, ezgrpc2_status status,
                                        const char *message) {
  if (call->stream == nullptr)
    return EZGRPC2_ERR_CLOSED;
  char grpc_status[16];
  int status_len = snprintf(grpc_status, sizeof(grpc_status), "%d", (int)status);
  if (message == nullptr && status != EZGRPC2_STATUS_OK)
    message = ezgrpc2_status_str(status);

  size_t message_len = 0;
  char *encoded = nullptr;
  if (message != nullptr) {
    encoded = encode_grpc_message(message, &message_len);
    if (encoded == nullptr)
      return EZGRPC2_ERR_NOMEM;
  }

  /* grpc-status, grpc-message and the application's trailers */
  size_t total = 1 + (encoded != nullptr) + call->trailers.len;
  nghttp2_nv *nva = ezgrpc2_malloc(total * sizeof(*nva));
  if (nva == nullptr) {
    ezgrpc2_free(encoded);
    return EZGRPC2_ERR_NOMEM;
  }
  size_t n = 0;
  nva[n++] = (nghttp2_nv){(uint8_t *)"grpc-status", (uint8_t *)grpc_status, 11,
                          (size_t)status_len, NGHTTP2_NV_FLAG_NONE};
  if (encoded != nullptr)
    nva[n++] = (nghttp2_nv){(uint8_t *)"grpc-message", (uint8_t *)encoded, 12, message_len,
                            NGHTTP2_NV_FLAG_NONE};
  if (call->trailers.len > 0)
    memcpy(nva + n, call->trailers.nva, call->trailers.len * sizeof(*nva));
  n += call->trailers.len;

  ezgrpc2_err err = EZGRPC2_OK;
  ezgrpc2_stream *stream = call->stream;
  if (!call->headers_sent) {
    /* nothing was sent: a single Trailers-Only HEADERS */
    err = submit_response_headers(call, NGHTTP2_FLAG_END_STREAM, nva, n);
  } else if (stream->is_data_active) {
    /* the trailers must not overtake the queued DATA. the data provider
     * submits them once it has written the last message */
    stream->pending_trailers = nva_dup(nva, n);
    stream->nb_pending_trailers = n;
    if (stream->pending_trailers == nullptr)
      err = EZGRPC2_ERR_NOMEM;
  } else if (nghttp2_submit_trailer(call->session->ngsession, call->stream_id, nva, n) != 0) {
    err = EZGRPC2_ERR_PROTOCOL;
  }
  ezgrpc2_free(nva);
  ezgrpc2_free(encoded);
  if (err != EZGRPC2_OK)
    return err;

  call->finished = true;
  stream->is_deadline_set = false;
  nv_list_free(&call->trailers);
  session_schedule_flush(call->session);
  interceptors_run_on_done(call, status);
  return EZGRPC2_OK;
}

void call_abort(ezgrpc2_call *call, ezgrpc2_status status, const char *message) {
  if (call->finished || call->closed)
    return;
  (void)call_finish_internal(call, status, message);
  call_cancel(call, status);
}

static ezgrpc2_err call_check(const ezgrpc2_call *call) {
  if (call == nullptr)
    return EZGRPC2_ERR_INVALID;
  if (call->finished)
    return EZGRPC2_ERR_FINISHED;
  if (call->closed)
    return EZGRPC2_ERR_CLOSED;
  return EZGRPC2_OK;
}

EZGRPC2_API ezgrpc2_err ezgrpc2_call_send_metadata(ezgrpc2_call *call,
                                                   const ezgrpc2_metadata *metadata,
                                                   size_t count) {
  ezgrpc2_err err = call_check(call);
  if (err != EZGRPC2_OK)
    return err;
  if (call->headers_sent)
    return EZGRPC2_ERR_STATE;
  /* sent with the first message, or with the status */
  return nv_list_add_metadata(&call->headers, metadata, count);
}

EZGRPC2_API ezgrpc2_err ezgrpc2_call_add_trailers(ezgrpc2_call *call,
                                                  const ezgrpc2_metadata *metadata,
                                                  size_t count) {
  ezgrpc2_err err = call_check(call);
  if (err != EZGRPC2_OK)
    return err;
  return nv_list_add_metadata(&call->trailers, metadata, count);
}

EZGRPC2_API ezgrpc2_err ezgrpc2_call_send(ezgrpc2_call *call, ezgrpc2_message *message) {
  ezgrpc2_err err = message != nullptr ? call_check(call) : EZGRPC2_ERR_INVALID;
  if (err != EZGRPC2_OK) {
    ezgrpc2_message_free(message);
    return err;
  }

  if (!call->headers_sent) {
    call->compression = message->cflag ? message->compression : EZGRPC2_COMPRESS_IDENTITY;
    err = submit_response_headers(call, NGHTTP2_FLAG_NONE, nullptr, 0);
    if (err != EZGRPC2_OK) {
      ezgrpc2_message_free(message);
      return err;
    }
  } else if (message->cflag != 0 &&
             (call->compression == EZGRPC2_COMPRESS_IDENTITY || message->compression != call->compression)) {
    /* the encoding was announced with the first message */
    ezgrpc2_message_free(message);
    return EZGRPC2_ERR_STATE;
  }

  ezgrpc2_stream *stream = call->stream;
  if (ezgrpc2_list_push_back(&stream->lqueue_omessages, message) != 0) {
    ezgrpc2_message_free(message);
    return EZGRPC2_ERR_NOMEM;
  }

  /* an active data provider picks up the new message by itself */
  if (!stream->is_data_active) {
    nghttp2_data_provider data_provider = {
        .source.ptr = stream,
        .read_callback = data_source_read_callback2,
    };
    if (nghttp2_submit_data(call->session->ngsession, NGHTTP2_FLAG_NONE, call->stream_id,
                            &data_provider) != 0)
      return EZGRPC2_ERR_PROTOCOL;
    stream->is_data_active = true;
  }
  session_schedule_flush(call->session);
  return EZGRPC2_OK;
}

EZGRPC2_API ezgrpc2_err ezgrpc2_call_send_data(ezgrpc2_call *call, const void *data,
                                               size_t len) {
  if (call == nullptr || (data == nullptr && len > 0))
    return EZGRPC2_ERR_INVALID;
  ezgrpc2_message *message = ezgrpc2_message_new(data, len);
  if (message == nullptr)
    return EZGRPC2_ERR_NOMEM;
  return ezgrpc2_call_send(call, message);
}

EZGRPC2_API ezgrpc2_err ezgrpc2_call_finish(ezgrpc2_call *call, ezgrpc2_status status,
                                            const char *message) {
  ezgrpc2_err err = call_check(call);
  if (err != EZGRPC2_OK)
    return err;
  return call_finish_internal(call, status, message);
}

/* ---- accessors ---------------------------------------------------------------- */

EZGRPC2_API const ezgrpc2_message *ezgrpc2_call_request(const ezgrpc2_call *call) {
  return call != nullptr ? call->request : nullptr;
}

EZGRPC2_API const char *ezgrpc2_call_method(const ezgrpc2_call *call) {
  return call != nullptr ? call->route->path : nullptr;
}

EZGRPC2_API const char *ezgrpc2_call_peer(const ezgrpc2_call *call) {
  return call != nullptr ? call->peer : nullptr;
}

EZGRPC2_API const char *ezgrpc2_call_metadata(const ezgrpc2_call *call, const char *key,
                                              size_t *len) {
  if (call == nullptr || key == nullptr)
    return nullptr;
  size_t klen = strlen(key);
  for (size_t i = 0; i < call->nb_req_headers; i++) {
    const ezgrpc2_header *h = &call->req_headers[i];
    if (h->namelen == klen && strncasecmp(h->name, key, klen) == 0) {
      if (len != nullptr)
        *len = h->valuelen;
      return h->value;
    }
  }
  return nullptr;
}

EZGRPC2_API size_t ezgrpc2_call_metadata_count(const ezgrpc2_call *call) {
  return call != nullptr ? call->nb_req_headers : 0;
}

EZGRPC2_API bool ezgrpc2_call_metadata_at(const ezgrpc2_call *call, size_t index,
                                          ezgrpc2_metadata *out) {
  if (call == nullptr || out == nullptr || index >= call->nb_req_headers)
    return false;
  const ezgrpc2_header *h = &call->req_headers[index];
  *out = (ezgrpc2_metadata){
      .key = h->name,
      .key_len = h->namelen,
      .value = h->value,
      .value_len = h->valuelen,
  };
  return true;
}

EZGRPC2_API int64_t ezgrpc2_call_deadline(const ezgrpc2_call *call) {
  if (call == nullptr || call->deadline_ms == 0)
    return -1;
  u64 now = ezgrpc2_time_now_ms();
  return now >= call->deadline_ms ? 0 : (int64_t)(call->deadline_ms - now);
}

EZGRPC2_API bool ezgrpc2_call_is_cancelled(const ezgrpc2_call *call) {
  return call != nullptr && call->cancelled;
}

EZGRPC2_API ezgrpc2_status ezgrpc2_call_cancel_reason(const ezgrpc2_call *call) {
  return call != nullptr && call->cancelled ? call->cancel_reason : EZGRPC2_STATUS_OK;
}

EZGRPC2_API ezgrpc2_server *ezgrpc2_call_server(const ezgrpc2_call *call) {
  return call != nullptr ? call->server : nullptr;
}

EZGRPC2_API void ezgrpc2_call_set_userdata(ezgrpc2_call *call, void *data,
                                           void (*free_fn)(void *data)) {
  if (call == nullptr)
    return;
  if (call->userdata_free != nullptr && call->userdata != data)
    call->userdata_free(call->userdata);
  call->userdata = data;
  call->userdata_free = free_fn;
}

EZGRPC2_API void *ezgrpc2_call_userdata(const ezgrpc2_call *call) {
  return call != nullptr ? call->userdata : nullptr;
}

EZGRPC2_API void ezgrpc2_call_on_cancel(ezgrpc2_call *call,
                                        void (*fn)(ezgrpc2_call *call, void *userdata),
                                        void *userdata) {
  if (call == nullptr)
    return;
  call->cancel_fn = fn;
  call->cancel_userdata = userdata;
}

/* ---- from other threads -------------------------------------------------------- */

typedef struct async_op async_op;
struct async_op {
  ezgrpc2_call *call;
  ezgrpc2_message *message;
  bool finish;
  ezgrpc2_status status;
  char *text;
};

static void run_async_op(void *arg) {
  async_op *op = arg;
  if (op->finish)
    (void)ezgrpc2_call_finish(op->call, op->status, op->text);
  else
    (void)ezgrpc2_call_send(op->call, op->message);
  ezgrpc2_call_unref(op->call);
  ezgrpc2_free(op->text);
  ezgrpc2_free(op);
}

static ezgrpc2_err post_async_op(async_op *op) {
  /* the caller holds a reference, so the call is alive to take another */
  ezgrpc2_call_ref(op->call);
  ezgrpc2_err err = ezgrpc2_server_post(op->call->server, run_async_op, op);
  if (err != EZGRPC2_OK) {
    ezgrpc2_call_unref(op->call);
    ezgrpc2_message_free(op->message);
    ezgrpc2_free(op->text);
    ezgrpc2_free(op);
  }
  return err;
}

EZGRPC2_API ezgrpc2_err ezgrpc2_call_send_async(ezgrpc2_call *call, ezgrpc2_message *message) {
  if (call == nullptr || message == nullptr) {
    ezgrpc2_message_free(message);
    return EZGRPC2_ERR_INVALID;
  }
  async_op *op = ezgrpc2_calloc(1, sizeof(*op));
  if (op == nullptr) {
    ezgrpc2_message_free(message);
    return EZGRPC2_ERR_NOMEM;
  }
  op->call = call;
  op->message = message;
  return post_async_op(op);
}

EZGRPC2_API ezgrpc2_err ezgrpc2_call_finish_async(ezgrpc2_call *call, ezgrpc2_status status,
                                                  const char *message) {
  if (call == nullptr)
    return EZGRPC2_ERR_INVALID;
  async_op *op = ezgrpc2_calloc(1, sizeof(*op));
  if (op == nullptr)
    return EZGRPC2_ERR_NOMEM;
  op->call = call;
  op->finish = true;
  op->status = status;
  if (message != nullptr && (op->text = ezgrpc2_strdup(message)) == nullptr) {
    ezgrpc2_free(op);
    return EZGRPC2_ERR_NOMEM;
  }
  return post_async_op(op);
}

/* ---- dispatch ------------------------------------------------------------------ */

static void push_event(ezgrpc2_server *server, ezgrpc2_event_type type, ezgrpc2_call *call,
                       ezgrpc2_message *message) {
  if (server->nb_events == server->cap_events) {
    size_t cap = server->cap_events ? server->cap_events * 2 : 64;
    ezgrpc2_event *events = ezgrpc2_realloc(server->events, cap * sizeof(*events));
    if (events == nullptr) {
      LOG_ERROR(server, "out of memory, dropping an event\n");
      ezgrpc2_message_free(message);
      return;
    }
    server->events = events;
    server->cap_events = cap;
  }
  server->events[server->nb_events++] = (ezgrpc2_event){
      .type = type,
      .call = ezgrpc2_call_ref(call),
      .message = message,
      .userdata = call->route->userdata,
  };
}

void server_clear_events(ezgrpc2_server *server) {
  for (size_t i = 0; i < server->nb_events; i++) {
    ezgrpc2_message_free((ezgrpc2_message *)server->events[i].message);
    ezgrpc2_call_unref(server->events[i].call);
  }
  server->nb_events = 0;
}

static void dispatch_unary(ezgrpc2_call *call) {
  if (call->finished || call->closed) {
    drop_pending(call);
    return;
  }
  /* wait for the whole request */
  if (call->request_delivered || !call->half_closed)
    return;

  size_t count = ezgrpc2_list_count(&call->pending);
  if (count != 1) {
    drop_pending(call);
    (void)call_finish_internal(call, EZGRPC2_STATUS_INTERNAL,
                               count == 0 ? "missing request message"
                                          : "more than one request message");
    return;
  }
  call->request = ezgrpc2_list_pop_front(&call->pending);
  call->request_delivered = true;
  call->route->handler(call, call->request, call->route->userdata);
}

static void dispatch_stream(ezgrpc2_call *call) {
  const ezgrpc2_stream_handler *handler = &call->route->stream;
  ezgrpc2_message *msg;
  while (!call->finished && (msg = ezgrpc2_list_pop_front(&call->pending)) != nullptr) {
    if (handler->on_message != nullptr)
      handler->on_message(call, msg, call->route->userdata);
    ezgrpc2_message_free(msg);
  }
  /* finished calls take no more messages */
  drop_pending(call);
  if (call->half_closed && !call->end_delivered && !call->finished) {
    call->end_delivered = true;
    if (handler->on_end != nullptr)
      handler->on_end(call, call->route->userdata);
  }
}

static void dispatch_events(ezgrpc2_call *call) {
  ezgrpc2_server *server = call->server;
  ezgrpc2_message *msg;
  while (!call->finished && (msg = ezgrpc2_list_pop_front(&call->pending)) != nullptr)
    push_event(server, EZGRPC2_EVENT_MESSAGE, call, msg);
  drop_pending(call);
  if (call->half_closed && !call->end_delivered && !call->finished) {
    call->end_delivered = true;
    push_event(server, EZGRPC2_EVENT_END, call, nullptr);
  }
  if (call->cancelled && !call->cancel_delivered) {
    call->cancel_delivered = true;
    push_event(server, EZGRPC2_EVENT_CANCEL, call, nullptr);
    if (call->cancel_fn != nullptr)
      call->cancel_fn(call, call->cancel_userdata);
  }
}

static void dispatch_call(ezgrpc2_call *call) {
  if (!call->started) {
    /* cancelled before the application saw it */
    if (call->cancelled) {
      drop_pending(call);
      return;
    }
    call->started = true;
    if (call->route->kind == ROUTE_STREAM && call->route->stream.on_cancel != nullptr) {
      call->cancel_fn = call->route->stream.on_cancel;
      call->cancel_userdata = call->route->userdata;
    }
    if (!interceptors_run_on_call(call))
      call->rejected = true;
  }
  if (call->rejected) {
    drop_pending(call);
    return;
  }

  switch (call->route->kind) {
  case ROUTE_UNARY:
    dispatch_unary(call);
    break;
  case ROUTE_STREAM:
    dispatch_stream(call);
    break;
  case ROUTE_EVENTS:
    dispatch_events(call);
    return;
  }

  /* a unary call is cancelled for the application once its handler ran */
  bool seen = call->route->kind != ROUTE_UNARY || call->request_delivered;
  if (call->cancelled && !call->cancel_delivered && seen) {
    call->cancel_delivered = true;
    if (call->cancel_fn != nullptr)
      call->cancel_fn(call, call->cancel_userdata);
  }
}

void server_dispatch(ezgrpc2_server *server) {
  ezgrpc2_call *call;
  while ((call = server->ready_head) != nullptr) {
    server->ready_head = call->ready_next;
    if (server->ready_head == nullptr)
      server->ready_tail = nullptr;
    call->ready_next = nullptr;
    call->queued = false;
    dispatch_call(call);
    ezgrpc2_call_unref(call);
  }
}

void server_drop_ready(ezgrpc2_server *server) {
  ezgrpc2_call *call;
  while ((call = server->ready_head) != nullptr) {
    server->ready_head = call->ready_next;
    call->ready_next = nullptr;
    call->queued = false;
    ezgrpc2_call_unref(call);
  }
  server->ready_tail = nullptr;
}
