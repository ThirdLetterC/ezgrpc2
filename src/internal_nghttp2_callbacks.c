#include <stdio.h>
#include <string.h>
#include <assert.h>
#include <errno.h>

#include "core.h"
#include "ezgrpc2_events_struct.h"
#include "ezgrpc2_grpc_status.h"
#include "ezgrpc2_timeout.h"
#include "ezgrpc2_compression.h"
#include "ezgrpc2_tls.h"

#include "internal_helpers.h"
#include "internal_nghttp2_callbacks.h"

#define atlog(...) (void)0

static size_t parse_grpc_message(void *data, size_t len, ezgrpc2_messages **messages) {
  i8 *wire = data;
  *messages = nullptr;
  /* quick check if there's any message to parse */
  if (len < 5)
    return 0;

  size_t seek = 0, lseek = 0;
  while (seek < len) {
    if (seek + 5 > len) {
      return lseek;
    }
    i8 cflag = wire[seek];
    seek += 1;

    uint32_t msg_len = ntohl(uread_u32(wire + seek));
    seek += 4;
    void *msg_data = wire + seek;

    if (seek + msg_len > len) {
      return lseek;
    }

    if (*messages == nullptr)
      *messages = ezgrpc2_messages_new(nullptr);
    ezgrpc2_message *msg = nullptr;
    if (cflag == 1) {
      uint8_t *decomp = nullptr;
      size_t decomp_len = 0;
      if (ezgrpc2_decompress_gzip((const uint8_t *)msg_data, msg_len, &decomp, &decomp_len) == 0) {
        msg = ezgrpc2_message_new(0, decomp, decomp_len);
        ezgrpc2_free(decomp);
      } else {
        msg = ezgrpc2_message_new(cflag, msg_data, msg_len);
      }
    } else {
      msg = ezgrpc2_message_new(cflag, msg_data, msg_len);
    }
    assert(msg != nullptr);
    ezgrpc2_messages_push(*messages, msg);
    seek += msg_len;
    lseek = seek;
  }

  return lseek;
}

/* ----------- BEGIN NGHTTP2 CALLBACKS ------------------*/

ssize_t data_source_read_callback2(nghttp2_session *ngsession, i32 stream_id, u8 *buf,
                                   size_t buf_len, u32 *data_flags, nghttp2_data_source *source,
                                   [[maybe_unused]] void *user_data) {
#ifdef EZGRPC2_DEBUG
  ezgrpc2_session *ezsession = user_data;
#endif
  ezgrpc2_stream *ezstream = source->ptr;
  ezgrpc2_list *lmessages = &ezstream->lqueue_omessages;
  ezgrpc2_message *msg;

  /* each message goes out as a 5 byte prefix followed by its data.
   * trunc_seek tracks how much of the front message was already written,
   * so a message (and even its prefix) may span several DATA frames. */
  size_t buf_seek = 0;
  while (buf_seek < buf_len && (msg = ezgrpc2_list_peek_front(lmessages)) != nullptr) {
    size_t seek = ezstream->trunc_seek;
    size_t total = 5 + (size_t)msg->len;

    if (seek < 5) {
      u8 prefix[5];
      prefix[0] = msg->cflag;
      uint32_t len = htonl(msg->len);
      memcpy(prefix + 1, &len, 4);
      size_t n = 5 - seek < buf_len - buf_seek ? 5 - seek : buf_len - buf_seek;
      memcpy(buf + buf_seek, prefix + seek, n);
      buf_seek += n;
      seek += n;
    }
    if (seek >= 5) {
      size_t n = total - seek < buf_len - buf_seek ? total - seek : buf_len - buf_seek;
      memcpy(buf + buf_seek, msg->data + (seek - 5), n);
      buf_seek += n;
      seek += n;
    }

    if (seek == total) {
      (void)ezgrpc2_list_pop_front(lmessages);
      ezgrpc2_message_free(msg);
      seek = 0;
    }
    ezstream->trunc_seek = seek;
  }

  if (ezgrpc2_list_is_empty(lmessages)) {
    /* the stream stays open for the trailers */
    *data_flags = NGHTTP2_DATA_FLAG_EOF | NGHTTP2_DATA_FLAG_NO_END_STREAM;
    ezstream->is_data_active = false;
    if (ezstream->pending_trailers != nullptr) {
      if (nghttp2_submit_trailer(ngsession, stream_id, ezstream->pending_trailers,
                                 ezstream->nb_pending_trailers) != 0) {
        return NGHTTP2_ERR_CALLBACK_FAILURE;
      }
      free(ezstream->pending_trailers);
      ezstream->pending_trailers = nullptr;
      ezstream->nb_pending_trailers = 0;
    }
  }

  EZGRPC2_LOG_DEBUG(ezsession->server, "read done %zu bytes\n", buf_seek);
  return (ssize_t)buf_seek;
}

/* we're beginning to receive a header; open up memory for the new stream */
static int on_begin_headers_callback(nghttp2_session *session, const nghttp2_frame *frame,
                                     void *user_data) {
  ezgrpc2_session *ezsession = user_data;
  ezgrpc2_stream *ezstream = stream_new(frame->hd.stream_id);
  if (ezstream == nullptr)
    return NGHTTP2_ERR_CALLBACK_FAILURE;

  stream_link(ezsession, ezstream);
  nghttp2_session_set_stream_user_data(session, frame->hd.stream_id, ezstream);

  EZGRPC2_LOG_TRACE(ezsession->server, "stream id %d\n", frame->hd.stream_id);
  return 0;
}

static int on_header_callback2([[maybe_unused]] nghttp2_session *session,
                               const nghttp2_frame *frame, nghttp2_rcbuf *name_rcbuf,
                               nghttp2_rcbuf *value_rcbuf, [[maybe_unused]] u8 flags,
                               void *user_data) {
  ezgrpc2_session *ezsession = user_data;
  ezgrpc2_stream *ezstream =
      nghttp2_session_get_stream_user_data(ezsession->ngsession, frame->hd.stream_id);
  if (frame->hd.type != NGHTTP2_HEADERS || ezstream == nullptr)
    return 0;

  if (ezstream->nb_headers + 1 > CONFIG_STREAM_MAX_HEADERS) {
    EZGRPC2_LOG_ERROR(ezsession->server, "stream max headers exceeded\n");
    return 1;
  }
  nghttp2_vec vname = nghttp2_rcbuf_get_buf(name_rcbuf);
  nghttp2_vec vvalue = nghttp2_rcbuf_get_buf(value_rcbuf);
  const u8 *name = vname.base, *value = vvalue.base;
  size_t namelen = vname.len, valuelen = vvalue.len;

  /* keep nghttp2's buffers alive instead of copying them */
  nghttp2_rcbuf_incref(name_rcbuf);
  nghttp2_rcbuf_incref(value_rcbuf);
  ezstream->header_rcbufs[ezstream->nb_headers][0] = name_rcbuf;
  ezstream->header_rcbufs[ezstream->nb_headers][1] = value_rcbuf;
  ezstream->headers[ezstream->nb_headers].name = (char *)name;
  ezstream->headers[ezstream->nb_headers].value = (char *)value;
  ezstream->headers[ezstream->nb_headers].namelen = namelen;
  ezstream->headers[ezstream->nb_headers].valuelen = valuelen;
  ezstream->nb_headers++;

  const char *method = ":method";
  const char *scheme = ":scheme";
  const char *path = ":path";
  /* clang-format off */
#define EZSTRCMP(str1, str2) !(strlen(str1) == str2##len && !memcmp(str1, str2, str2##len))
  if (!EZSTRCMP(method, name))
    ezstream->is_method_post = !EZSTRCMP("POST", value);
  else if (!EZSTRCMP(scheme, name))
    ezstream->is_scheme_http = (!EZSTRCMP("http", value) || !EZSTRCMP("https", value));
  else if (!EZSTRCMP(path, name)) {
    for (size_t i = 0; i < ezsession->server->nb_paths; i++) 
      if (!EZSTRCMP(ezsession->server->paths[i].path, value)) {
        ezstream->path = &ezsession->server->paths[i];
        EZGRPC2_LOG_TRACE(ezsession->server, "found service %s\n", ezstream->path->path);
        break;
      }
    if (ezstream->path == nullptr)
      atlog("path not found %.*s\n", (int)valuelen, value);
  }
  else if (!EZSTRCMP("content-type", name)) {
    bool is_gt = valuelen >= 16;
    ezstream->hcontent_type = (const char *)value;
    ezstream->is_content_grpc = is_gt ? !strncmp("application/grpc", (const char *)value, 16) : false;
  }
  else if (!EZSTRCMP("grpc-encoding", name)) {
    ezstream->hgrpc_encoding = (const char *)value;
  }
  else if (!EZSTRCMP("grpc-accept-encoding", name)) {
    ezstream->hgrpc_accept_encoding = (const char *)value;
  }
  else if (!EZSTRCMP("grpc-timeout", name)) {
    ezstream->hgrpc_timeout = (const char *)value;
  }
  else if (!EZSTRCMP("te", name)) {
    ezstream->is_te_trailers = !EZSTRCMP("trailers", value);
  }
  else {
    ;
  }
#undef EZSTRCMP
  /* clang-format on */

  return 0;
}

static inline int on_frame_recv_headers(ezgrpc2_session *ezsession, const nghttp2_frame *frame) {
  ezgrpc2_stream *ezstream =
      nghttp2_session_get_stream_user_data(ezsession->ngsession, frame->hd.stream_id);
  assert(ezstream != nullptr);
  if (!(frame->hd.flags & NGHTTP2_FLAG_END_HEADERS)) {
    return 0;
  }

  if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM && !ezstream->is_content_grpc) {
    nghttp2_nv nva[1] = {
        {.name = (uint8_t *)":status",
         .namelen = 7,
         .value = (uint8_t *)"415",
         .valuelen = 3,
         .flags = NGHTTP2_NV_FLAG_NONE},
    };
    if (nghttp2_submit_headers(ezsession->ngsession, NGHTTP2_FLAG_END_STREAM, ezstream->stream_id,
                               nullptr, nva, 1, nullptr)) {
      assert(0);
    }

    return 0;
  }

  if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) {
    EZGRPC2_LOG_DEBUG(ezsession->server, "received a header with an end stream. sending rst\n");
    nghttp2_submit_rst_stream(ezsession->ngsession, NGHTTP2_FLAG_NONE, frame->hd.stream_id, 1);
    return 0;
  }

  if (!(ezstream->is_method_post && ezstream->is_scheme_http && ezstream->is_te_trailers &&
        ezstream->is_content_grpc)) {
    EZGRPC2_LOG_TRACE(ezsession->server,
                      "is_method_post %d && is_scheme_http %d && is_te_trailers %d && "
                      "is_content_grpc %d. sending rst\n",
                      ezstream->is_method_post, ezstream->is_scheme_http, ezstream->is_te_trailers,
                      ezstream->is_content_grpc);
    nghttp2_submit_rst_stream(ezsession->ngsession, NGHTTP2_FLAG_NONE, ezstream->stream_id, 1);
    return 0;
  }
  bool client_accepts_gzip =
      ezstream->hgrpc_accept_encoding && strstr(ezstream->hgrpc_accept_encoding, "gzip");
  nghttp2_nv nva[4] = {
      {.name = (uint8_t *)":status",
       .namelen = 7,
       .value = (uint8_t *)"200",
       .valuelen = 3,
       .flags = NGHTTP2_NV_FLAG_NONE},
      {.name = (uint8_t *)"content-type",
       .namelen = 12,
       .value = (uint8_t *)"application/grpc",
       .valuelen = 16,
       .flags = NGHTTP2_NV_FLAG_NONE},
      {.name = (uint8_t *)"grpc-accept-encoding",
       .namelen = 20,
       .value = (uint8_t *)"gzip,identity",
       .valuelen = 13,
       .flags = NGHTTP2_NV_FLAG_NONE},
      {.name = (uint8_t *)"grpc-encoding",
       .namelen = 13,
       .value = (uint8_t *)"gzip",
       .valuelen = 4,
       .flags = NGHTTP2_NV_FLAG_NONE},
  };

  size_t nv_count = client_accepts_gzip ? 4 : 3;
  if (nghttp2_submit_headers(ezsession->ngsession, 0, ezstream->stream_id, nullptr, nva, nv_count,
                             nullptr)) {
    EZGRPC2_LOG_TRACE(ezsession->server, "nghttp2_submit_headers failed\n");
    return 1;
  }

  if (ezstream->hgrpc_timeout != nullptr) {
    int64_t ms = ezgrpc2_timeout_parse_ms(ezstream->hgrpc_timeout);
    if (ms > 0) {
      ezstream->is_deadline_set = true;
      ezstream->deadline_ms = ezgrpc2_time_now_ms() + (uint64_t)ms;
      if (ezstream->deadline_ms < ezsession->server->next_deadline_ms)
        ezsession->server->next_deadline_ms = ezstream->deadline_ms;
    }
  }

  if (ezstream->path == nullptr) {
    atlog("path null, submitting req\n");
    int status = EZGRPC2_GRPC_STATUS_UNIMPLEMENTED;
    char grpc_status[32] = {};
    int len = snprintf(grpc_status, sizeof(grpc_status), "%d", status);
    const char *grpc_message = ezgrpc2_grpc_status_strstatus(status);
    nghttp2_nv trailers[] = {
        {(uint8_t *)"grpc-status", (uint8_t *)grpc_status, 11, (size_t)len, NGHTTP2_NV_FLAG_NONE},
        {(uint8_t *)"grpc-message", (uint8_t *)grpc_message, 12, strlen(grpc_message),
         NGHTTP2_NV_FLAG_NONE},
    };
    nghttp2_submit_trailer(ezsession->ngsession, frame->hd.stream_id, trailers, 2);
    EZGRPC2_LOG_TRACE(ezsession->server, "path null. trailer submitted\n");
    return 0;
  }

  EZGRPC2_LOG_TRACE(ezsession->server, "ret id %d\n", frame->hd.stream_id);
  return 0;
}

static inline int on_frame_recv_settings(ezgrpc2_session *ezsession, const nghttp2_frame *frame) {
  if (frame->hd.stream_id != 0) {
    EZGRPC2_LOG_DEBUG(ezsession->server,
                      "received a SETTINGS frame at stream_id 0. immediately returning\n");
    return 0;
  }
  if (frame->settings.hd.flags & NGHTTP2_FLAG_ACK && frame->settings.hd.length == 0) {
    /* OK. The client acknowledge our settings. do nothing */
  } else {
    atlog("sent an rst\n");
    nghttp2_submit_rst_stream(ezsession->ngsession, NGHTTP2_FLAG_NONE, frame->hd.stream_id, 1);
  }
  return 0;
}

static inline int on_frame_recv_data(ezgrpc2_session *ezsession, const nghttp2_frame *frame) {
  ezgrpc2_stream *ezstream =
      nghttp2_session_get_stream_user_data(ezsession->ngsession, frame->hd.stream_id);
  if (ezstream == nullptr) {
    atlog("stream %d not found. sending rst\n", frame->hd.stream_id);
    nghttp2_submit_rst_stream(ezsession->ngsession, NGHTTP2_FLAG_NONE, frame->hd.stream_id, 1);
    return 0;
  }
  if (ezstream->path == nullptr) {
    EZGRPC2_LOG_TRACE(ezsession->server, "path null\n");
    return 0;
  }

  if (frame->hd.flags & NGHTTP2_FLAG_END_STREAM && ezstream->recv_len == 0) {
    ezgrpc2_event event;
    event.type = EZGRPC2_EVENT_MESSAGE;
    event.session_uuid = ezsession->session_uuid;
    event.userdata = ezstream->path->userdata;
    event.message.stream_id = frame->hd.stream_id;
    event.message.end_stream = 1;
    event.message.messages = ezgrpc2_messages_new(nullptr);
    ezgrpc2_events_write(ezsession->server->events, event);

    return 0;
  }

  do {
    ezgrpc2_messages *messages;
    size_t lseek = parse_grpc_message(ezstream->recv_data, ezstream->recv_len, &messages);
    if (lseek) {
      assert(lseek <= ezstream->recv_len);
      EZGRPC2_LOG_TRACE(ezsession->server, "event message %zu\n", ezgrpc2_messages_count(messages));
      memmove(ezstream->recv_data, ezstream->recv_data + lseek, ezstream->recv_len - lseek);
      ezstream->recv_len -= lseek;

      ezgrpc2_event event;
      event.type = EZGRPC2_EVENT_MESSAGE;
      event.session_uuid = ezsession->session_uuid;
      event.userdata = ezstream->path->userdata;
      event.message.messages = messages;
      event.message.end_stream =
          (frame->hd.flags & NGHTTP2_FLAG_END_STREAM) && ezstream->recv_len == 0;
      event.message.stream_id = frame->hd.stream_id;
      ezgrpc2_events_write(ezsession->server->events, event);
    }
  } while (0);

  if ((frame->hd.flags & NGHTTP2_FLAG_END_STREAM) && ezstream->recv_len) {
    atlog("event dataloss\n");
    ezgrpc2_event event;
    event.type = EZGRPC2_EVENT_DATALOSS;
    event.session_uuid = ezsession->session_uuid;
    event.userdata = ezstream->path->userdata;
    event.dataloss.stream_id = ezstream->stream_id;

    ezgrpc2_events_write(ezsession->server->events, event);
  }
  return 0;
}

/* the client cancelled the call (or hit its deadline) by resetting the stream.
 * nghttp2 invokes this before on_stream_close_callback, so ezstream is still alive. */
static inline int on_frame_recv_rst_stream(ezgrpc2_session *ezsession, ezgrpc2_stream *ezstream,
                                           const nghttp2_frame *frame) {
  if (ezstream == nullptr || ezstream->path == nullptr) {
    return 0;
  }
  ezgrpc2_event event = {};
  event.type = EZGRPC2_EVENT_CANCEL;
  event.session_uuid = ezsession->session_uuid;
  event.userdata = ezstream->path->userdata;
  event.cancel.stream_id = frame->hd.stream_id;
  ezgrpc2_events_write(ezsession->server->events, event);
  return 0;
}

static int on_frame_send_callback([[maybe_unused]] nghttp2_session *session,
                                  const nghttp2_frame *frame, [[maybe_unused]] void *user_data) {
#ifdef EZGRPC2_TRACE
  ezgrpc2_session *ezsession = user_data;
#endif

  switch (frame->hd.type) {
  case NGHTTP2_SETTINGS:
    EZGRPC2_LOG_TRACE(ezsession->server, "> FRAME[SETTINGS, sid=%d, ack=%d]\n", frame->hd.stream_id,
                      !!(frame->hd.flags & NGHTTP2_FLAG_ACK));
    return 0;
  case NGHTTP2_HEADERS:
    EZGRPC2_LOG_TRACE(ezsession->server, "> FRAME[HEADERS, sid=%d, eos=%d]\n", frame->hd.stream_id,
                      !!(frame->hd.flags & NGHTTP2_FLAG_END_STREAM));
    for (size_t i = 0; i < frame->headers.nvlen; i++)
      EZGRPC2_LOG_TRACE(ezsession->server, "  %s: %s\n", frame->headers.nva[i].name,
                        frame->headers.nva[i].value);
    return 0;
  case NGHTTP2_DATA:
    EZGRPC2_LOG_TRACE(ezsession->server, "> FRAME[DATA, sid=%d, eos=%d, len=%zu]\n",
                      frame->hd.stream_id, !!(frame->hd.flags & NGHTTP2_FLAG_END_STREAM),
                      frame->hd.length);
    return 0;
  case NGHTTP2_WINDOW_UPDATE:
    EZGRPC2_LOG_TRACE(ezsession->server,
                      "> FRAME[WINDOW_UPDATE, sid=%d, eos=%d, len=%zu, wsi=%d]\n",
                      frame->hd.stream_id, !!(frame->hd.flags & NGHTTP2_FLAG_END_STREAM),
                      frame->hd.length, frame->window_update.window_size_increment);
    break;
  default:
    break;
  }
  return 0;
}

static int on_frame_recv_callback([[maybe_unused]] nghttp2_session *session,
                                  const nghttp2_frame *frame, void *user_data) {
  ezgrpc2_session *ezsession = user_data;
  ezgrpc2_stream *ezstream =
      nghttp2_session_get_stream_user_data(ezsession->ngsession, frame->hd.stream_id);
  switch (frame->hd.type) {
  case NGHTTP2_SETTINGS:
    EZGRPC2_LOG_TRACE(ezsession->server, "< FRAME[SETTINGS, sid=%d, ack=%d]\n", frame->hd.stream_id,
                      !!(frame->hd.flags & NGHTTP2_FLAG_ACK));
    return on_frame_recv_settings(ezsession, frame);
  case NGHTTP2_HEADERS:
    EZGRPC2_LOG_TRACE(ezsession->server, "< FRAME[HEADERS, sid=%d, eos=%d]\n", frame->hd.stream_id,
                      !!(frame->hd.flags & NGHTTP2_FLAG_END_STREAM));
    if (ezstream != nullptr) {
      for (size_t i = 0; i < ezstream->nb_headers; i++)
        EZGRPC2_LOG_TRACE(ezsession->server, "  %s: %s\n", ezstream->headers[i].name,
                          ezstream->headers[i].value);
    }
    return on_frame_recv_headers(ezsession, frame);
  case NGHTTP2_DATA:
    EZGRPC2_LOG_TRACE(ezsession->server, "< FRAME[DATA, sid=%d, eos=%d, len=%zu]\n",
                      frame->hd.stream_id, !!(frame->hd.flags & NGHTTP2_FLAG_END_STREAM),
                      frame->hd.length);
    return on_frame_recv_data(ezsession, frame);
  case NGHTTP2_RST_STREAM:
    EZGRPC2_LOG_TRACE(ezsession->server, "< FRAME[RST_STREAM, sid=%d, error_code=%u]\n",
                      frame->hd.stream_id, frame->rst_stream.error_code);
    return on_frame_recv_rst_stream(ezsession, ezstream, frame);
  case NGHTTP2_WINDOW_UPDATE:
    EZGRPC2_LOG_TRACE(ezsession->server,
                      "< FRAME[WINDOW_UPDATE, sid=%d, eos=%d, len=%zu, wsi=%d]\n",
                      frame->hd.stream_id, !!(frame->hd.flags & NGHTTP2_FLAG_END_STREAM),
                      frame->hd.length, frame->window_update.window_size_increment);
    break;
  default:
    break;
  }
  return 0;
}

static int on_data_chunk_recv_callback(nghttp2_session *session, [[maybe_unused]] u8 flags,
                                       i32 stream_id, const u8 *data, size_t len, void *user_data) {
  ezgrpc2_session *ezsession = user_data;
  ezgrpc2_stream *ezstream = nghttp2_session_get_stream_user_data(session, stream_id);
  if (ezstream == nullptr) {
    atlog("stream_id %d doesn't exist\n", stream_id);
    return 0;
  }

  /* prevent an attacker from pushing large POST data */
  if ((ezstream->recv_len + len) > ezsession->server->http2_settings.initial_window_size) {
    nghttp2_submit_rst_stream(ezsession->ngsession, NGHTTP2_FLAG_NONE, stream_id, 1);
    EZGRPC2_LOG_ERROR(ezsession->server,
                      "POST data exceeds maximum allowed size, killing stream %d\n", stream_id);
    return 1;
  }

  if (ezstream->recv_len + len > ezstream->recv_cap) {
    size_t cap = ezstream->recv_cap * 2;
    while (cap < ezstream->recv_len + len)
      cap *= 2;
    u8 *recv_data = malloc(cap);
    if (recv_data == nullptr)
      return NGHTTP2_ERR_CALLBACK_FAILURE;
    memcpy(recv_data, ezstream->recv_data, ezstream->recv_len);
    if (ezstream->recv_data != ezstream->recv_inline)
      free(ezstream->recv_data);
    ezstream->recv_data = recv_data;
    ezstream->recv_cap = cap;
  }
  memcpy(ezstream->recv_data + ezstream->recv_len, data, len);
  ezstream->recv_len += len;
  EZGRPC2_LOG_TRACE(ezsession->server, "data chunk recv %zu bytes at stream %d\n", len, stream_id);

  return 0;
}

static int on_stream_close_callback(nghttp2_session *session, i32 stream_id,
                                    [[maybe_unused]] u32 error_code, void *user_data) {
  atlog("stream %d closed\n", stream_id);
  ezgrpc2_session *ezsession = user_data;
  ezgrpc2_stream *ezstream = nghttp2_session_get_stream_user_data(session, stream_id);
  if (ezstream == nullptr)
    return 0;
  stream_unlink(ezsession, ezstream);
  stream_free(ezstream);

  return 0;
}

/* ----------- END NGHTTP2 CALLBACKS ------------------*/

void server_setup_ngcallbacks(nghttp2_session_callbacks *ngcallbacks) {
  /* no send/recv callbacks: the event loop moves the bytes with
   * nghttp2_session_mem_recv() and nghttp2_session_mem_send() */

  /* prepares and allocates memory for the incoming HEADERS frame */
  nghttp2_session_callbacks_set_on_begin_headers_callback(ngcallbacks, on_begin_headers_callback);
  /* we received that HEADERS frame. now here's the http2 fields */
  nghttp2_session_callbacks_set_on_header_callback2(ngcallbacks, on_header_callback2);
  /* we received a frame. do something about it */
  nghttp2_session_callbacks_set_on_frame_recv_callback(ngcallbacks, on_frame_recv_callback);
  nghttp2_session_callbacks_set_on_frame_send_callback(ngcallbacks, on_frame_send_callback);

  nghttp2_session_callbacks_set_on_data_chunk_recv_callback(ngcallbacks,
                                                            on_data_chunk_recv_callback);
  nghttp2_session_callbacks_set_on_stream_close_callback(ngcallbacks, on_stream_close_callback);
}
