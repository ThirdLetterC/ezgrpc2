#ifndef EZGRPC2_INTERNAL_HELPERS_H
#define EZGRPC2_INTERNAL_HELPERS_H

#include <stdio.h>
#include <string.h>
#include "core.h"

int makenonblock(int sockfd);

void session_close(ezgrpc2_session *session);
ezgrpc2_session *session_find(ezgrpc2_session *ezsessions, size_t nb_ezsessions,
                              ezgrpc2_session_uuid session_uuid);

/* queues the session to have its pending nghttp2 output written on the next poll */
void session_schedule_flush(ezgrpc2_session *session);
/* serializes pending nghttp2 frames and writes them to the socket */
void session_flush(ezgrpc2_session *session);
void server_flush_sessions(ezgrpc2_server *server);
void on_listener_connection(uv_stream_t *listener, int status);

void stream_free(ezgrpc2_stream *stream);
ezgrpc2_stream *stream_new(i32 stream_id);
void stream_link(ezgrpc2_session *session, ezgrpc2_stream *stream);
void stream_unlink(ezgrpc2_session *session, ezgrpc2_stream *stream);

int list_cmp_ezheader_name(const void *data, const void *userdata);
void ezlog(const char *fmt, ...);
void ezflog(FILE *fp, const char *fmt, ...);
char *ezgetdt(char *buf, size_t len);

/* ensures `extra` more bytes fit in `buf` */
static inline int buf_reserve(ezgrpc2_buf *buf, size_t extra) {
  if (buf->cap - buf->len >= extra)
    return 0;
  size_t cap = buf->cap ? buf->cap : 4096;
  while (cap - buf->len < extra)
    cap *= 2;
  u8 *data = realloc(buf->data, cap);
  if (data == nullptr)
    return 1;
  buf->data = data;
  buf->cap = cap;
  return 0;
}

static inline int buf_append(ezgrpc2_buf *buf, const void *data, size_t len) {
  if (buf_reserve(buf, len))
    return 1;
  memcpy(buf->data + buf->len, data, len);
  buf->len += len;
  return 0;
}

static inline void buf_free(ezgrpc2_buf *buf) {
  free(buf->data);
  *buf = (ezgrpc2_buf){};
}

/* unaligned read unsigned 32 */
static inline uint32_t uread_u32(const void *p) {
  uint32_t ret;
  memcpy(&ret, p, 4);
  return ret;
}

#endif
