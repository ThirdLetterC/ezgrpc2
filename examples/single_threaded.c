/* The author disclaims copyright to this source code
 * and releases it into the public domain.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>
#include <signal.h>

#include "ezgrpc2.h"

struct path_userdata_t {
  char is_unary;
  ezgrpc2_messages *(*callback)(ezgrpc2_messages *);
};

_Atomic int pp = 0;
ezgrpc2_messages *callback_path0([[maybe_unused]] ezgrpc2_messages *messages) {
  ezgrpc2_messages *messages_ret = ezgrpc2_messages_new(nullptr);
  /* pretend this is our response (output messages) */
  for (int i = 0; i < 2; i++, pp++) {
    ezgrpc2_message *msg = ezgrpc2_message_new(0, (void *)"    path0!", 11);
    *(uint32_t*)(msg->data) = pp;
    
    ezgrpc2_messages_push(messages_ret, msg);
  }

  return messages_ret;
}

ezgrpc2_messages *callback_path1([[maybe_unused]] ezgrpc2_messages *messages) {
  ezgrpc2_messages *messages_ret = ezgrpc2_messages_new(nullptr);
  for (int i = 0; i < 2; i++, pp++) {
    ezgrpc2_message *msg = ezgrpc2_message_new(0, (void *)"    path1!", 11);
    *(uint32_t*)(msg->data) = pp;
    ezgrpc2_messages_push(messages_ret, msg);
  }

  return messages_ret;
}

static void handle_event_message(ezgrpc2_event *event,
                                 ezgrpc2_server *server) {
  size_t nb_msg = ezgrpc2_messages_count(event->message.messages);
  struct path_userdata_t *path_userdata = event->userdata;
  if (path_userdata->is_unary) {
    if (!event->message.end_stream || nb_msg != 1) {
      /* This is unary service, but they are sending more than one message */
      ezgrpc2_server_session_stream_end(server, event->session_uuid,
                                 event->message.stream_id,
                                 EZGRPC2_GRPC_STATUS_INVALID_ARGUMENT);
      return;
    }
  }
  ezgrpc2_messages *messages = path_userdata->callback(event->message.messages);
  switch (ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, messages)) {
    case 0:
      if (event->message.end_stream)
        ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_OK);
      break;
    case 1:
      break;
    case 2:
      printf(">stream id doesn't exist %d\n", event->message.stream_id);
      break;
    case 3:
      printf("fatal\n");
      break;
    default:
      assert(0);
  }
  ezgrpc2_messages_free(messages);
}

static void handle_event_dataloss(ezgrpc2_event *event,
                                  ezgrpc2_server *server) {
  ezgrpc2_server_session_stream_end(server, event->session_uuid, event->dataloss.stream_id, EZGRPC2_GRPC_STATUS_DATA_LOSS);
}

static void handle_events(ezgrpc2_server *server, ezgrpc2_events *events) {
  ezgrpc2_event *event;
  size_t nb_event;
  /* check for events in the paths */
  while ((event = ezgrpc2_events_read(events, &nb_event)) != nullptr) {
    for (size_t i = 0; i < nb_event; i++) {
      switch (event[i].type) {
      case EZGRPC2_EVENT_MESSAGE:
        handle_event_message(&event[i], server);
        break;
      case EZGRPC2_EVENT_DATALOSS:
        handle_event_dataloss(&event[i], server);
        break;
      case EZGRPC2_EVENT_CANCEL:
        printf("event cancel on stream %d\n\n", event[i].cancel.stream_id);
        break;
      case EZGRPC2_EVENT_CONNECT:
        printf("event connect\n");
        break;
      case EZGRPC2_EVENT_DISCONNECT:
        printf("event disconnect\n");
        break;
      }
    }
    ezgrpc2_free(event);
  }
}

int main(void) {
  (void)ezgrpc2_global_init(0);
  int res = 0;
  const size_t nb_paths = 2;
  struct path_userdata_t path_userdata[nb_paths];

  signal(SIGPIPE, SIG_IGN);

  /* The heart of this API */
  ezgrpc2_server *server = ezgrpc2_server_new("0.0.0.0", 19009, "::", 19009, 16, nullptr, nullptr);
  assert(server != nullptr);

  /* we expect to receive one or more grpc message in a single stream */
  path_userdata[0].is_unary = 0;
  path_userdata[0].callback = callback_path0;

  /* we expect to receive only one grpc message in a single stream */
  path_userdata[1].is_unary = 1;
  path_userdata[1].callback = callback_path1;

  /* services */
  (void)ezgrpc2_server_register_path(server, "/test.yourAPI/whatever_service1", path_userdata, 0, 0);
  (void)ezgrpc2_server_register_path(server, "/test.yourAPI/whatever_service2", path_userdata + 1, 0, 0);

  ezgrpc2_events *events = ezgrpc2_events_new(nullptr);
  while (1) {
    int timeout = 10000;

    // step 1. server poll
    if ((res = ezgrpc2_server_poll(server, events, timeout)) < 0)
      break;

    if (res > 0) {
      puts("incoming events");
      handle_events(server, events);
    } else if (res == 0) {
      printf("no event\n");
    }
  }

  if (res < 0) {
    printf("poll err\n");
  }

  ezgrpc2_events_free(events);
  ezgrpc2_server_free(server);
  ezgrpc2_global_cleanup();

  return res;
}
