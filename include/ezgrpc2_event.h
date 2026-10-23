#ifndef EZGRPC2_EVENT_H
#define EZGRPC2_EVENT_H

#include "ezgrpc2_call.h"
#include "ezgrpc2_defs.h"
#include "ezgrpc2_message.h"

/**
 * Events of the methods registered with ezgrpc2_server_register_path(), read
 * with ezgrpc2_server_events() after ezgrpc2_server_poll(). Methods registered
 * with a handler are dispatched to it instead.
 */
typedef enum ezgrpc2_event_type {
  /** A request message arrived: ``message`` is set. */
  EZGRPC2_EVENT_MESSAGE,
  /** The client finished sending (half-close). */
  EZGRPC2_EVENT_END,
  /** The call was cancelled; see ezgrpc2_call_cancel_reason(). */
  EZGRPC2_EVENT_CANCEL,
} ezgrpc2_event_type;

typedef struct ezgrpc2_event {
  ezgrpc2_event_type type;
  /** The call. Valid until the next poll. */
  ezgrpc2_call *call;
  /** EZGRPC2_EVENT_MESSAGE: the message. Valid until the next poll. */
  const ezgrpc2_message *message;
  /** The userdata given to ezgrpc2_server_register_path(). */
  void *userdata;
} ezgrpc2_event;

#endif /* EZGRPC2_EVENT_H */
