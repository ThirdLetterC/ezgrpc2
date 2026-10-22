#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <signal.h>
#include <assert.h>
#include <unistd.h>

#include "ezgrpc2.h"
#include "helloworld.pb-c.h"

static volatile sig_atomic_t g_running = 1;
static int g_quiet = 0;

static void sig_handler([[maybe_unused]] int sig) {
  g_running = 0;
}

typedef struct method_handler_t {
  const char *name;
  ezgrpc2_messages *(*handler)(const ezgrpc2_message *req_msg);
} method_handler_t;

static ezgrpc2_messages *handle_say_hello(const ezgrpc2_message *req_msg) {
  Helloworld__HelloRequest *req = EZGRPC2_PB_UNPACK(
      Helloworld__HelloRequest, &helloworld__hello_request__descriptor, req_msg);
  if (req == nullptr) {
    fprintf(stderr, "[server] Failed to unpack HelloRequest\n");
    return nullptr;
  }

  const char *name = (req->name && req->name[0] != '\0') ? req->name : "World";
  if (!g_quiet) {
    printf("[server] Received SayHello request for name='%s'\n", name);
  }

  char reply_buf[256];
  snprintf(reply_buf, sizeof(reply_buf), "Hello, %s!", name);

  Helloworld__HelloReply reply = HELLOWORLD__HELLO_REPLY__INIT;
  reply.message = reply_buf;

  ezgrpc2_messages *res_msgs = ezgrpc2_pb_create_response((ProtobufCMessage *)&reply);
  EZGRPC2_PB_FREE_UNPACKED(req);
  return res_msgs;
}

static ezgrpc2_messages *handle_say_hello_again(const ezgrpc2_message *req_msg) {
  Helloworld__HelloRequest *req = EZGRPC2_PB_UNPACK(
      Helloworld__HelloRequest, &helloworld__hello_request__descriptor, req_msg);
  if (req == nullptr) {
    fprintf(stderr, "[server] Failed to unpack HelloRequest (again)\n");
    return nullptr;
  }

  const char *name = (req->name && req->name[0] != '\0') ? req->name : "World";
  if (!g_quiet) {
    printf("[server] Received SayHelloAgain request for name='%s'\n", name);
  }

  char reply_buf[256];
  snprintf(reply_buf, sizeof(reply_buf), "Hello again, %s!", name);

  Helloworld__HelloReply reply = HELLOWORLD__HELLO_REPLY__INIT;
  reply.message = reply_buf;

  ezgrpc2_messages *res_msgs = ezgrpc2_pb_create_response((ProtobufCMessage *)&reply);
  EZGRPC2_PB_FREE_UNPACKED(req);
  return res_msgs;
}

static void handle_event_message(ezgrpc2_event *event, ezgrpc2_server *server) {
  method_handler_t *handler = event->userdata;
  size_t count = ezgrpc2_messages_count(event->message.messages);

  if (count == 0) {
    /* Trailing empty frame (e.g., END_STREAM flag on empty frame) */
    return;
  }

  if (count != 1) {
    /* Unary RPC expects exactly one request message */
    ezgrpc2_server_session_stream_end(server, event->session_uuid,
                                     event->message.stream_id,
                                     EZGRPC2_GRPC_STATUS_INVALID_ARGUMENT);
    return;
  }

  ezgrpc2_message *req_msg = ezgrpc2_messages_pop(event->message.messages);
  ezgrpc2_messages *resp_msgs = handler->handler(req_msg);
  ezgrpc2_message_free(req_msg);

  if (resp_msgs == nullptr) {
    ezgrpc2_server_session_stream_end(server, event->session_uuid,
                                     event->message.stream_id,
                                     EZGRPC2_GRPC_STATUS_INTERNAL);
    return;
  }

  int send_res = ezgrpc2_server_session_stream_send(
      server, event->session_uuid, event->message.stream_id, resp_msgs);
  if (send_res == 0) {
    ezgrpc2_server_session_stream_end(
        server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_OK);
  } else {
    fprintf(stderr, "[server] Stream send failed: %d\n", send_res);
  }

  ezgrpc2_messages_free(resp_msgs);
}

static void handle_events(ezgrpc2_server *server, ezgrpc2_events *events) {
  ezgrpc2_event *event;
  size_t nb_event;

  while ((event = ezgrpc2_events_read(events, &nb_event)) != nullptr) {
    for (size_t i = 0; i < nb_event; i++) {
      switch (event[i].type) {
      case EZGRPC2_EVENT_MESSAGE:
        handle_event_message(&event[i], server);
        break;
      case EZGRPC2_EVENT_DATALOSS:
        ezgrpc2_server_session_stream_end(
            server, event[i].session_uuid, event[i].dataloss.stream_id,
            EZGRPC2_GRPC_STATUS_DATA_LOSS);
        break;
      case EZGRPC2_EVENT_CANCEL:
        printf("[server] Stream %d cancelled by client\n", event[i].cancel.stream_id);
        break;
      case EZGRPC2_EVENT_CONNECT:
        if (!g_quiet) printf("[server] New client connection\n");
        break;
      case EZGRPC2_EVENT_DISCONNECT:
        if (!g_quiet) printf("[server] Client disconnected\n");
        break;
      }
    }
    ezgrpc2_free(event);
  }
}

int main(int argc, char *argv[]) {
  int port = 19009;
  if (argc > 1) {
    port = atoi(argv[1]);
    if (port <= 0 || port > 65535) port = 19009;
  }
  if (argc > 2) {
    g_quiet = atoi(argv[2]);
  }

  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);

  (void)ezgrpc2_global_init(0);

  method_handler_t say_hello_handler = {
      .name = "SayHello",
      .handler = handle_say_hello,
  };

  method_handler_t say_hello_again_handler = {
      .name = "SayHelloAgain",
      .handler = handle_say_hello_again,
  };

  ezgrpc2_server *server = ezgrpc2_server_new(
      "0.0.0.0", port, "::", port, 16, nullptr, nullptr);
  if (server == nullptr) {
    fprintf(stderr, "Failed to create ezgrpc2 server on port %d\n", port);
    return 1;
  }

  printf("[server] Greeter gRPC server listening on port %d (IPv4 & IPv6)...\n", port);

  /* Register service methods according to gRPC naming convention: /<package>.<Service>/<Method> */
  (void)ezgrpc2_server_register_path(server, "/helloworld.Greeter/SayHello", &say_hello_handler, 0, 0);
  (void)ezgrpc2_server_register_path(server, "/helloworld.Greeter/SayHelloAgain", &say_hello_again_handler, 0, 0);

  ezgrpc2_events *events = ezgrpc2_events_new(nullptr);

  while (g_running) {
    int res = ezgrpc2_server_poll(server, events, 500);
    if (res > 0) {
      handle_events(server, events);
    } else if (res < 0) {
      break;
    }
  }

  printf("\n[server] Shutting down...\n");
  ezgrpc2_events_free(events);
  ezgrpc2_server_free(server);
  ezgrpc2_global_cleanup();
  return 0;
}
