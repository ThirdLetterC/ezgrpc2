/* A gRPC server whose messages are FlatBuffers, built and read with flatcc.
 *
 *   ./server.bin [port] [quiet]
 */
#include <signal.h>
#include <stdio.h>
#include <string.h>

#include "ezgrpc2.h"
#include "greeter_builder.h"
#include "greeter_verifier.h"

#define MAX_REPLIES 100

static volatile sig_atomic_t g_running = 1;
static int g_quiet = 0;

static void sig_handler([[maybe_unused]] int sig) {
  g_running = 0;
}

/* builds every response. ezgrpc2_fb_pack() resets it after each one */
static flatcc_builder_t g_builder;

typedef void (*handler_fn)(ezgrpc2_server *server, ezgrpc2_event *event,
                           greeter_HelloRequest_table_t req);

static void reply(ezgrpc2_server *server, ezgrpc2_event *event, const char *text, int32_t index) {
  greeter_HelloReply_start_as_root(&g_builder);
  greeter_HelloReply_message_create_str(&g_builder, text);
  greeter_HelloReply_index_add(&g_builder, index);
  greeter_HelloReply_end_as_root(&g_builder);

  ezgrpc2_messages *res = ezgrpc2_fb_create_response(&g_builder);
  ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, res);
  ezgrpc2_messages_free(res);
}

static const char *request_name(greeter_HelloRequest_table_t req) {
  const char *name = greeter_HelloRequest_name(req);
  return name != nullptr && name[0] != '\0' ? name : "World";
}

/* SayHello (unary) */
static void say_hello(ezgrpc2_server *server, ezgrpc2_event *event,
                      greeter_HelloRequest_table_t req) {
  char text[256];
  snprintf(text, sizeof(text), "Hello, %s!", request_name(req));
  if (!g_quiet)
    printf("[server] SayHello name='%s'\n", request_name(req));

  reply(server, event, text, 0);
  ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id,
                                    EZGRPC2_GRPC_STATUS_OK);
}

/* SayManyHellos (server streaming) */
static void say_many_hellos(ezgrpc2_server *server, ezgrpc2_event *event,
                            greeter_HelloRequest_table_t req) {
  int32_t count = greeter_HelloRequest_count(req);
  if (count < 0 || count > MAX_REPLIES) {
    ezgrpc2_server_session_stream_end2(server, event->session_uuid, event->message.stream_id,
                                       EZGRPC2_GRPC_STATUS_INVALID_ARGUMENT,
                                       "count must be between 0 and 100", nullptr, 0);
    return;
  }
  if (!g_quiet)
    printf("[server] SayManyHellos name='%s' count=%d\n", request_name(req), count);

  for (int32_t i = 0; i < count; i++) {
    char text[256];
    snprintf(text, sizeof(text), "Hello #%d, %s!", i, request_name(req));
    reply(server, event, text, i);
  }
  ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id,
                                    EZGRPC2_GRPC_STATUS_OK);
}

static void handle_message(ezgrpc2_server *server, ezgrpc2_event *event) {
  handler_fn handler = *(handler_fn *)event->userdata;

  /* both methods take a single request. it may arrive in an event of its own,
   * followed by an empty one that only carries end_stream */
  size_t nb_messages = ezgrpc2_messages_count(event->message.messages);
  if (nb_messages == 0)
    return;
  if (nb_messages > 1) {
    ezgrpc2_server_session_stream_end2(server, event->session_uuid, event->message.stream_id,
                                       EZGRPC2_GRPC_STATUS_INVALID_ARGUMENT,
                                       "expected exactly one request message", nullptr, 0);
    return;
  }

  /* the table points into the message: keep the message until done */
  ezgrpc2_message *msg = ezgrpc2_messages_peek_front(event->message.messages);
  greeter_HelloRequest_table_t req = EZGRPC2_FB_UNPACK(greeter_HelloRequest, msg);
  if (req == nullptr) {
    ezgrpc2_server_session_stream_end2(server, event->session_uuid, event->message.stream_id,
                                       EZGRPC2_GRPC_STATUS_INVALID_ARGUMENT,
                                       "request is not a valid greeter.HelloRequest", nullptr, 0);
    return;
  }
  handler(server, event, req);
}

int main(int argc, char *argv[]) {
  int port = argc > 1 ? atoi(argv[1]) : 19029;
  g_quiet = argc > 2 ? atoi(argv[2]) : 0;

  signal(SIGPIPE, SIG_IGN);
  signal(SIGINT, sig_handler);
  signal(SIGTERM, sig_handler);
  setvbuf(stdout, nullptr, _IONBF, 0);

  ezgrpc2_global_init(0);
  if (ezgrpc2_fb_builder_init(&g_builder) != 0)
    return 1;

  ezgrpc2_server *server =
      ezgrpc2_server_new("0.0.0.0", (uint16_t)port, "::", (uint16_t)port, 1024, nullptr, nullptr);
  if (server == nullptr) {
    fprintf(stderr, "Failed to create server on port %d\n", port);
    return 1;
  }

  static handler_fn say_hello_handler = say_hello;
  static handler_fn say_many_hellos_handler = say_many_hellos;
  ezgrpc2_server_register_path(server, "/greeter.Greeter/SayHello", &say_hello_handler, 0, 0);
  ezgrpc2_server_register_path(server, "/greeter.Greeter/SayManyHellos", &say_many_hellos_handler,
                               0, 0);
  printf("[server] FlatBuffers Greeter listening on port %d\n", port);

  ezgrpc2_events *events = ezgrpc2_events_new(nullptr);
  while (g_running) {
    if (ezgrpc2_server_poll(server, events, 500) < 0)
      break;

    ezgrpc2_event *block;
    size_t count;
    while ((block = ezgrpc2_events_read(events, &count)) != nullptr) {
      for (size_t i = 0; i < count; i++) {
        ezgrpc2_event *event = &block[i];
        if (event->type == EZGRPC2_EVENT_MESSAGE) {
          handle_message(server, event);
          /* the application owns the received messages */
          ezgrpc2_messages_free(event->message.messages);
        } else if (event->type == EZGRPC2_EVENT_DATALOSS) {
          ezgrpc2_server_session_stream_end(server, event->session_uuid, event->dataloss.stream_id,
                                            EZGRPC2_GRPC_STATUS_DATA_LOSS);
        }
      }
      ezgrpc2_free(block);
    }
  }

  printf("[server] Shutting down...\n");
  ezgrpc2_events_free(events);
  ezgrpc2_server_free(server);
  flatcc_builder_clear(&g_builder);
  ezgrpc2_global_cleanup();
  return 0;
}
