#ifndef EZGRPC2_CALL_H
#define EZGRPC2_CALL_H

#include "ezgrpc2_defs.h"
#include "ezgrpc2_error.h"
#include "ezgrpc2_message.h"
#include "ezgrpc2_metadata.h"
#include "ezgrpc2_status.h"

/**
 * A gRPC call (an HTTP/2 stream). Everything about one request goes through
 * its call: reading metadata, sending responses and finishing it.
 *
 * Lifetime: a call handed to a handler or found in an event stays valid until
 * the handler returns (or until the next ezgrpc2_server_poll() for events).
 * Take a reference with ezgrpc2_call_ref() to keep it longer, e.g. to answer
 * from another thread, and drop it with ezgrpc2_call_unref().
 *
 * A call outlives its stream: once the client is gone, operations on it fail
 * with EZGRPC2_ERR_CLOSED instead of crashing.
 *
 * Threads: every function here must be called from the thread running the
 * server, except ezgrpc2_call_ref(), ezgrpc2_call_unref(),
 * ezgrpc2_call_send_async() and ezgrpc2_call_finish_async().
 */
typedef struct ezgrpc2_call ezgrpc2_call;
typedef struct ezgrpc2_server ezgrpc2_server;

/* ---- the request ------------------------------------------------------ */

/**
 * The request of a method registered with ezgrpc2_server_handle(). It stays
 * valid as long as the call. nullptr for other methods.
 */
EZGRPC2_API const ezgrpc2_message *ezgrpc2_call_request(const ezgrpc2_call *call);

/** The method path, e.g. ``"/helloworld.Greeter/SayHello"``. */
EZGRPC2_API const char *ezgrpc2_call_method(const ezgrpc2_call *call);

/** The client address, e.g. ``"127.0.0.1:51234"`` or ``"[::1]:51234"``. */
EZGRPC2_API const char *ezgrpc2_call_peer(const ezgrpc2_call *call);

/**
 * Looks up request metadata by key (case-insensitive). Returns the value,
 * NULL-terminated and valid as long as the call, or nullptr if absent.
 * ``len`` may be nullptr.
 */
EZGRPC2_API const char *ezgrpc2_call_metadata(const ezgrpc2_call *call, const char *key,
                                              size_t *len);

/** The number of request headers, including pseudo-headers such as ``:path``. */
EZGRPC2_API size_t ezgrpc2_call_metadata_count(const ezgrpc2_call *call);

/** Reads the request header at ``index``. Returns false if out of range. */
EZGRPC2_API bool ezgrpc2_call_metadata_at(const ezgrpc2_call *call, size_t index,
                                          ezgrpc2_metadata *out);

/**
 * Milliseconds left until the client's deadline (``grpc-timeout``), 0 once it
 * passed, or -1 without a deadline. The library ends calls that run past
 * their deadline with DEADLINE_EXCEEDED.
 */
EZGRPC2_API int64_t ezgrpc2_call_deadline(const ezgrpc2_call *call);

/**
 * True once the call ended without the application finishing it: the client
 * cancelled or disconnected, the deadline passed, or a message was truncated.
 */
EZGRPC2_API bool ezgrpc2_call_is_cancelled(const ezgrpc2_call *call);

/**
 * Why the call was cancelled: CANCELLED (by the client), DEADLINE_EXCEEDED,
 * UNAVAILABLE (connection lost) or DATA_LOSS. EZGRPC2_STATUS_OK if it was not.
 */
EZGRPC2_API ezgrpc2_status ezgrpc2_call_cancel_reason(const ezgrpc2_call *call);

/** The server the call belongs to. */
EZGRPC2_API ezgrpc2_server *ezgrpc2_call_server(const ezgrpc2_call *call);

/* ---- per-call state ----------------------------------------------------- */

/**
 * Attaches application data to the call, e.g. the state of a streaming call.
 * ``free_fn`` (may be nullptr) is called on it when the call is released.
 */
EZGRPC2_API void ezgrpc2_call_set_userdata(ezgrpc2_call *call, void *data,
                                           void (*free_fn)(void *data));

/** Returns the data set with ezgrpc2_call_set_userdata(), or nullptr. */
EZGRPC2_API void *ezgrpc2_call_userdata(const ezgrpc2_call *call);

/**
 * Calls ``fn`` if the call is cancelled after this point (see
 * ezgrpc2_call_is_cancelled()). Useful for calls answered later.
 */
EZGRPC2_API void ezgrpc2_call_on_cancel(ezgrpc2_call *call,
                                        void (*fn)(ezgrpc2_call *call, void *userdata),
                                        void *userdata);

/* ---- answering ---------------------------------------------------------- */

/**
 * Sends initial metadata (response headers). Only before the first message.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_call_send_metadata(ezgrpc2_call *call,
                                                   const ezgrpc2_metadata *metadata,
                                                   size_t count);

/**
 * Sends a response message. Takes ownership of ``message``, also when it
 * fails. Call it several times to stream responses.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_call_send(ezgrpc2_call *call, ezgrpc2_message *message);

/**
 * Sends a copy of ``len`` bytes of ``data`` as a response message.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_call_send_data(ezgrpc2_call *call, const void *data,
                                               size_t len);

/**
 * Adds trailing metadata, sent by ezgrpc2_call_finish().
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_call_add_trailers(ezgrpc2_call *call,
                                                  const ezgrpc2_metadata *metadata,
                                                  size_t count);

/**
 * Ends the call with a status. ``message`` (may be nullptr) is sent as
 * ``grpc-message``; error statuses without one get the status name.
 * Every call must be finished exactly once, unless it is cancelled.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_call_finish(ezgrpc2_call *call, ezgrpc2_status status,
                                            const char *message);

/* ---- references and threads --------------------------------------------- */

/** Takes a reference, keeping the call valid. Thread-safe. Returns ``call``. */
EZGRPC2_API ezgrpc2_call *ezgrpc2_call_ref(ezgrpc2_call *call);

/** Drops a reference taken with ezgrpc2_call_ref(). Thread-safe. */
EZGRPC2_API void ezgrpc2_call_unref(ezgrpc2_call *call);

/**
 * Thread-safe ezgrpc2_call_send(): queues the message for the server thread
 * and wakes it up. The caller must hold a reference to the call.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_call_send_async(ezgrpc2_call *call, ezgrpc2_message *message);

/**
 * Thread-safe ezgrpc2_call_finish(). The caller must hold a reference.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_call_finish_async(ezgrpc2_call *call, ezgrpc2_status status,
                                                  const char *message);

#endif /* EZGRPC2_CALL_H */
