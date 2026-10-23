#pragma once

/* The TLS layer never touches the socket. Ciphertext received by the event
 * loop is handed over with grpc_tls_feed() and the ciphertext to send is
 * collected with grpc_tls_drain(), so every TLS library is driven the
 * same way. Exactly one backend (src/grpc_tls_<backend>.c) is built. */

#include "grpc_tls.h"
#include "internal_helpers.h"

/* The TLS backend's server context. */
typedef struct grpc_tls_ctx grpc_tls_ctx;

/* Per-connection TLS layer, used by the session code. These return:
 *   grpc_tls_session_init, _feed, _drain: 0 on success
 *   grpc_tls_handshake: 1 when complete, 0 while in progress, -1 on error
 *   grpc_tls_read: bytes read, 0 on close_notify, -2 when more input is needed, -1 on error
 *   grpc_tls_write: bytes written (all of them), -1 on error */
int grpc_tls_session_init(grpc_session *session);
int grpc_tls_feed(grpc_session *session, const void *buf, size_t len);
int grpc_tls_drain(grpc_session *session, grpc_buf *out);
int grpc_tls_handshake(grpc_session *session);
ssize_t grpc_tls_read(grpc_session *session, void *buf, size_t len);
ssize_t grpc_tls_write(grpc_session *session, const void *buf, size_t len);
void grpc_tls_close_notify(grpc_session *session);
void grpc_tls_session_free(grpc_session *session);

/* The backend contract. Same return conventions as above. */
typedef struct grpc_tls_session grpc_tls_session;

const char *tls_backend_name(void);
const char *tls_backend_version(void);
grpc_tls_ctx *tls_backend_ctx_new(const char *cert_file, const char *key_file);
void tls_backend_ctx_free(grpc_tls_ctx *ctx);
void *tls_backend_ctx_native(grpc_tls_ctx *ctx);
grpc_tls_session *tls_backend_session_new(grpc_tls_ctx *ctx);
void tls_backend_session_free(grpc_tls_session *tls);
int tls_backend_feed(grpc_tls_session *tls, const void *buf, size_t len);
int tls_backend_drain(grpc_tls_session *tls, grpc_buf *out);
int tls_backend_handshake(grpc_tls_session *tls);
ssize_t tls_backend_read(grpc_tls_session *tls, void *buf, size_t len);
ssize_t tls_backend_write(grpc_tls_session *tls, const void *buf, size_t len);
void tls_backend_close_notify(grpc_tls_session *tls);

/* Ciphertext queues for backends that move bytes through I/O callbacks
 * (wolfSSL, GnuTLS, rustls): the callbacks pull from `in` and push to `out`. */
typedef struct grpc_tls_io grpc_tls_io;
struct grpc_tls_io {
  grpc_buf in;
  size_t in_seek;
  grpc_buf out;
};

static inline int tls_io_feed(grpc_tls_io *io, const void *data, size_t len) {
  /* Drop the bytes already consumed by the library. */
  if (io->in_seek > 0) {
    memmove(io->in.data, io->in.data + io->in_seek, io->in.len - io->in_seek);
    io->in.len -= io->in_seek;
    io->in_seek = 0;
  }
  return buf_append(&io->in, data, len);
}

static inline size_t tls_io_pending(const grpc_tls_io *io) {
  return io->in.len - io->in_seek;
}

static inline size_t tls_io_pull(grpc_tls_io *io, void *dst, size_t len) {
  size_t n = tls_io_pending(io);
  if (n > len)
    n = len;
  memcpy(dst, io->in.data + io->in_seek, n);
  io->in_seek += n;
  return n;
}

static inline int tls_io_push(grpc_tls_io *io, const void *data, size_t len) {
  return buf_append(&io->out, data, len);
}

static inline int tls_io_drain(grpc_tls_io *io, grpc_buf *dst) {
  if (io->out.len == 0)
    return 0;
  if (buf_append(dst, io->out.data, io->out.len))
    return 1;
  io->out.len = 0;
  return 0;
}

static inline void tls_io_free(grpc_tls_io *io) {
  buf_free(&io->in);
  buf_free(&io->out);
}
