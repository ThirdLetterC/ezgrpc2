#ifndef EZGRPC2_INTERNAL_TLS_H
#define EZGRPC2_INTERNAL_TLS_H

/* The TLS layer never touches the socket. Ciphertext received by the event
 * loop is handed over with ezgrpc2_tls_feed() and the ciphertext to send is
 * collected with ezgrpc2_tls_drain(), so every TLS library is driven the
 * same way. Exactly one backend (src/ezgrpc2_tls_<backend>.c) is built. */

#include "ezgrpc2_tls.h"
#include "internal_helpers.h"

/* Per-connection TLS layer, used by the session code. These return:
 *   ezgrpc2_tls_session_init, _feed, _drain: 0 on success
 *   ezgrpc2_tls_handshake: 1 when complete, 0 while in progress, -1 on error
 *   ezgrpc2_tls_read: bytes read, 0 on close_notify, -2 when more input is needed, -1 on error
 *   ezgrpc2_tls_write: bytes written (all of them), -1 on error */
int ezgrpc2_tls_session_init(ezgrpc2_session *session);
int ezgrpc2_tls_feed(ezgrpc2_session *session, const void *buf, size_t len);
int ezgrpc2_tls_drain(ezgrpc2_session *session, ezgrpc2_buf *out);
int ezgrpc2_tls_handshake(ezgrpc2_session *session);
ssize_t ezgrpc2_tls_read(ezgrpc2_session *session, void *buf, size_t len);
ssize_t ezgrpc2_tls_write(ezgrpc2_session *session, const void *buf, size_t len);
void ezgrpc2_tls_close_notify(ezgrpc2_session *session);
void ezgrpc2_tls_session_free(ezgrpc2_session *session);

/* The backend contract. Same return conventions as above. */
typedef struct ezgrpc2_tls_session ezgrpc2_tls_session;

const char *tls_backend_name(void);
const char *tls_backend_version(void);
ezgrpc2_tls_ctx *tls_backend_ctx_new(const char *cert_file, const char *key_file);
void tls_backend_ctx_free(ezgrpc2_tls_ctx *ctx);
void *tls_backend_ctx_native(ezgrpc2_tls_ctx *ctx);
ezgrpc2_tls_session *tls_backend_session_new(ezgrpc2_tls_ctx *ctx);
void tls_backend_session_free(ezgrpc2_tls_session *tls);
int tls_backend_feed(ezgrpc2_tls_session *tls, const void *buf, size_t len);
int tls_backend_drain(ezgrpc2_tls_session *tls, ezgrpc2_buf *out);
int tls_backend_handshake(ezgrpc2_tls_session *tls);
ssize_t tls_backend_read(ezgrpc2_tls_session *tls, void *buf, size_t len);
ssize_t tls_backend_write(ezgrpc2_tls_session *tls, const void *buf, size_t len);
void tls_backend_close_notify(ezgrpc2_tls_session *tls);

/* Ciphertext queues for backends that move bytes through I/O callbacks
 * (wolfSSL, GnuTLS, rustls): the callbacks pull from `in` and push to `out`. */
typedef struct ezgrpc2_tls_io ezgrpc2_tls_io;
struct ezgrpc2_tls_io {
  ezgrpc2_buf in;
  size_t in_seek;
  ezgrpc2_buf out;
};

static inline int tls_io_feed(ezgrpc2_tls_io *io, const void *data, size_t len) {
  /* drop what the library already consumed */
  if (io->in_seek > 0) {
    memmove(io->in.data, io->in.data + io->in_seek, io->in.len - io->in_seek);
    io->in.len -= io->in_seek;
    io->in_seek = 0;
  }
  return buf_append(&io->in, data, len);
}

static inline size_t tls_io_pending(const ezgrpc2_tls_io *io) {
  return io->in.len - io->in_seek;
}

static inline size_t tls_io_pull(ezgrpc2_tls_io *io, void *dst, size_t len) {
  size_t n = tls_io_pending(io);
  if (n > len)
    n = len;
  memcpy(dst, io->in.data + io->in_seek, n);
  io->in_seek += n;
  return n;
}

static inline int tls_io_push(ezgrpc2_tls_io *io, const void *data, size_t len) {
  return buf_append(&io->out, data, len);
}

static inline int tls_io_drain(ezgrpc2_tls_io *io, ezgrpc2_buf *dst) {
  if (io->out.len == 0)
    return 0;
  if (buf_append(dst, io->out.data, io->out.len))
    return 1;
  io->out.len = 0;
  return 0;
}

static inline void tls_io_free(ezgrpc2_tls_io *io) {
  buf_free(&io->in);
  buf_free(&io->out);
}

#endif
