#pragma once

#include "grpc_call.h"
#include "grpc_defs.h"
#include "grpc_message.h"

/**
 * Events of the methods registered with grpc_server_register_path(), read
 * with grpc_server_events() after grpc_server_poll(). Methods registered
 * with a handler are dispatched to it instead.
 */
typedef enum grpc_event_type {
  /** A request message arrived: ``message`` is set. */
  GRPC_EVENT_MESSAGE,
  /** The client finished sending (half-close). */
  GRPC_EVENT_END,
  /** The call was cancelled; see grpc_call_cancel_reason(). */
  GRPC_EVENT_CANCEL,
} grpc_event_type;

typedef struct grpc_event {
  grpc_event_type type;
  /** The call. Valid until the next poll. */
  grpc_call *call;
  /** The message for GRPC_EVENT_MESSAGE; valid until the next poll. */
  const grpc_message *message;
  /** The userdata given to grpc_server_register_path(). */
  void *userdata;
} grpc_event;
