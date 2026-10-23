#pragma once

#include "grpc_defs.h"
#include "grpc_error.h"
#include "grpc_message.h"
#include "grpc_metadata.h"
#include "grpc_status.h"

/**
 * A gRPC call (an HTTP/2 stream). Everything about one request goes through
 * its call: reading metadata, sending responses and finishing it.
 *
 * Lifetime: a call handed to a handler or found in an event stays valid until
 * the handler returns (or until the next grpc_server_poll() for events).
 * Take a reference with grpc_call_ref() to keep it longer, e.g. to answer
 * from another thread, and drop it with grpc_call_unref().
 *
 * A call outlives its stream: once the client is gone, operations on it fail
 * with GRPC_ERR_CLOSED instead of crashing.
 *
 * Threads: every function here must be called from the thread running the
 * server, except grpc_call_ref(), grpc_call_unref(),
 * grpc_call_send_async() and grpc_call_finish_async().
 */
typedef struct grpc_call grpc_call;
typedef struct grpc_server grpc_server;

/* ---- the request ------------------------------------------------------ */

/**
 * The request of a method registered with grpc_server_handle(). It stays
 * valid as long as the call. It is nullptr for other methods.
 */
GRPC_API const grpc_message *grpc_call_request(const grpc_call *call);

/** The method path, e.g. ``"/helloworld.Greeter/SayHello"``. */
GRPC_API const char *grpc_call_method(const grpc_call *call);

/** The client address, e.g. ``"127.0.0.1:51234"`` or ``"[::1]:51234"``. */
GRPC_API const char *grpc_call_peer(const grpc_call *call);

/**
 * Looks up request metadata by key (case-insensitive). Returns the value,
 * NUL-terminated and valid as long as the call, or nullptr if absent.
 * ``len`` may be nullptr.
 */
GRPC_API const char *grpc_call_metadata(const grpc_call *call, const char *key,
                                              size_t *len);

/** The number of request headers, including pseudo-headers such as ``:path``. */
GRPC_API size_t grpc_call_metadata_count(const grpc_call *call);

/** Reads the request header at ``index``. Returns false if out of range. */
GRPC_API bool grpc_call_metadata_at(const grpc_call *call, size_t index,
                                          grpc_metadata *out);

/**
 * Milliseconds left until the client's deadline (``grpc-timeout``), 0 once it
 * passed, or -1 without a deadline. The library ends calls that run past
 * their deadline with DEADLINE_EXCEEDED.
 */
GRPC_API int64_t grpc_call_deadline(const grpc_call *call);

/**
 * True once the call ended without the application finishing it: the client
 * cancelled or disconnected, the deadline passed, or a message was truncated.
 */
GRPC_API bool grpc_call_is_cancelled(const grpc_call *call);

/**
 * Why the call was cancelled: CANCELLED (by the client), DEADLINE_EXCEEDED,
 * UNAVAILABLE (connection lost) or DATA_LOSS. GRPC_STATUS_OK if it was not.
 */
GRPC_API grpc_status grpc_call_cancel_reason(const grpc_call *call);

/** The server the call belongs to. */
GRPC_API grpc_server *grpc_call_server(const grpc_call *call);

/* ---- per-call state ----------------------------------------------------- */

/**
 * Attaches application data to the call, e.g. the state of a streaming call.
 * ``free_fn`` (may be nullptr) is called on it when the call is released.
 */
GRPC_API void grpc_call_set_userdata(grpc_call *call, void *data,
                                           void (*free_fn)(void *data));

/** Returns the data set with grpc_call_set_userdata(), or nullptr. */
GRPC_API void *grpc_call_userdata(const grpc_call *call);

/**
 * Calls ``fn`` if the call is cancelled after this point (see
 * grpc_call_is_cancelled()). Useful for calls answered later.
 */
GRPC_API void grpc_call_on_cancel(grpc_call *call,
                                        void (*fn)(grpc_call *call, void *userdata),
                                        void *userdata);

/* ---- answering ---------------------------------------------------------- */

/**
 * Sends initial metadata (response headers). Only before the first message.
 */
GRPC_API grpc_err grpc_call_send_metadata(grpc_call *call,
                                                   const grpc_metadata *metadata,
                                                   size_t count);

/**
 * Sends a response message. Takes ownership of ``message``, also when it
 * fails. Call it several times to stream responses.
 */
GRPC_API grpc_err grpc_call_send(grpc_call *call, grpc_message *message);

/**
 * Sends a copy of ``len`` bytes of ``data`` as a response message.
 */
GRPC_API grpc_err grpc_call_send_data(grpc_call *call, const void *data,
                                               size_t len);

/**
 * Adds trailing metadata, sent by grpc_call_finish().
 */
GRPC_API grpc_err grpc_call_add_trailers(grpc_call *call,
                                                  const grpc_metadata *metadata,
                                                  size_t count);

/**
 * Ends the call with a status. ``message`` (may be nullptr) is sent as
 * ``grpc-message``; error statuses without one get the status name.
 * Every call must be finished exactly once, unless it is cancelled.
 */
GRPC_API grpc_err grpc_call_finish(grpc_call *call, grpc_status status,
                                            const char *message);

/* ---- references and threads --------------------------------------------- */

/** Takes a reference, keeping the call valid. Thread-safe. Returns ``call``. */
GRPC_API grpc_call *grpc_call_ref(grpc_call *call);

/** Drops a reference taken with grpc_call_ref(). Thread-safe. */
GRPC_API void grpc_call_unref(grpc_call *call);

/**
 * Thread-safe grpc_call_send(): queues the message for the server thread
 * and wakes it up. The caller must hold a reference to the call.
 */
GRPC_API grpc_err grpc_call_send_async(grpc_call *call, grpc_message *message);

/**
 * Thread-safe grpc_call_finish(). The caller must hold a reference.
 */
GRPC_API grpc_err grpc_call_finish_async(grpc_call *call, grpc_status status,
                                                  const char *message);
