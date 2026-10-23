#ifndef EZGRPC2_INTERCEPTOR_H
#define EZGRPC2_INTERCEPTOR_H

#include "ezgrpc2_call.h"
#include "ezgrpc2_defs.h"
#include "ezgrpc2_error.h"
#include "ezgrpc2_status.h"

typedef struct ezgrpc2_server ezgrpc2_server;

/**
 * Interceptors run for every call, in the order they were added.
 *
 * ``on_call`` runs before the handler (or the first event) of a call, when
 * its metadata is available. Returning anything but EZGRPC2_STATUS_OK ends
 * the call with that status and ``*message`` (if set) instead of running it.
 *
 * ``on_done`` runs when the call ends: finished by the application or the
 * library, or cancelled. ``duration_ms`` is the time since the request arrived.
 */
typedef struct ezgrpc2_interceptor {
  const char *name;
  ezgrpc2_status (*on_call)(ezgrpc2_call *call, const char **message, void *userdata);
  void (*on_done)(ezgrpc2_call *call, ezgrpc2_status status, uint64_t duration_ms,
                  void *userdata);
  void *userdata;
  /** Called on ``userdata`` when the server is freed. Optional. */
  void (*userdata_free)(void *userdata);
} ezgrpc2_interceptor;

/**
 * Adds an interceptor. The struct is copied.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_server_add_interceptor(ezgrpc2_server *server,
                                                       const ezgrpc2_interceptor *interceptor);

/** Returns true if ``token`` (from ``authorization: Bearer <token>``) is valid. */
typedef bool (*ezgrpc2_token_validator_fn)(const char *token, void *userdata);

/**
 * An interceptor that ends calls with UNAUTHENTICATED unless they carry
 * ``authorization: Bearer <token>`` accepted by ``validator``.
 */
EZGRPC2_API ezgrpc2_interceptor ezgrpc2_interceptor_bearer_auth(ezgrpc2_token_validator_fn validator,
                                                                void *userdata);

#endif /* EZGRPC2_INTERCEPTOR_H */
