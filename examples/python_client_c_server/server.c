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

// Client streaming accumulator per stream
#define MAX_CLIENT_STREAM_REQS 16
typedef struct client_stream_state_t {
  int32_t stream_id;
  size_t count;
  char names[MAX_CLIENT_STREAM_REQS][64];
} client_stream_state_t;

static client_stream_state_t s_client_streams[32];
static size_t s_client_streams_count = 0;

static client_stream_state_t *get_or_create_client_stream(int32_t stream_id) {
  for (size_t i = 0; i < s_client_streams_count; i++) {
    if (s_client_streams[i].stream_id == stream_id) {
      return &s_client_streams[i];
    }
  }
  if (s_client_streams_count < 32) {
    client_stream_state_t *s = &s_client_streams[s_client_streams_count++];
    memset(s, 0, sizeof(*s));
    s->stream_id = stream_id;
    return s;
  }
  return nullptr;
}

static void remove_client_stream(int32_t stream_id) {
  for (size_t i = 0; i < s_client_streams_count; i++) {
    if (s_client_streams[i].stream_id == stream_id) {
      s_client_streams[i] = s_client_streams[--s_client_streams_count];
      return;
    }
  }
}

// Handler prototypes
typedef struct method_handler_t {
  const char *name;
  void (*handler)(ezgrpc2_server *server, ezgrpc2_event *event);
} method_handler_t;

// 1. SayHello (Unary)
static void handle_say_hello(ezgrpc2_server *server, ezgrpc2_event *event) {
  size_t count = ezgrpc2_messages_count(event->message.messages);
  if (count == 0) return;

  ezgrpc2_message *req_msg = ezgrpc2_messages_pop(event->message.messages);
  Helloworld__HelloRequest *req = EZGRPC2_PB_UNPACK(
      Helloworld__HelloRequest, &helloworld__hello_request__descriptor, req_msg);
  ezgrpc2_message_free(req_msg);

  if (req == nullptr) {
    ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_INTERNAL);
    return;
  }

  const char *name = (req->name && req->name[0] != '\0') ? req->name : "World";
  if (!g_quiet) printf("[server] Received SayHello request for name='%s'\n", name);

  char reply_buf[256];
  snprintf(reply_buf, sizeof(reply_buf), "Hello, %s!", name);

  Helloworld__HelloReply reply = HELLOWORLD__HELLO_REPLY__INIT;
  reply.message = reply_buf;

  ezgrpc2_messages *res_msgs = ezgrpc2_pb_create_response((ProtobufCMessage *)&reply);
  EZGRPC2_PB_FREE_UNPACKED(req);

  ezgrpc2_metadata_entry trailers[1] = {
      {.key = "x-custom-reply", .value = "pong", .key_len = 14, .value_len = 4}
  };

  ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, res_msgs);
  ezgrpc2_server_session_stream_end2(server, event->session_uuid, event->message.stream_id,
                                     EZGRPC2_GRPC_STATUS_OK, nullptr, trailers, 1);
  ezgrpc2_messages_free(res_msgs);
}

// 2. SayHelloAgain (Unary)
static void handle_say_hello_again(ezgrpc2_server *server, ezgrpc2_event *event) {
  size_t count = ezgrpc2_messages_count(event->message.messages);
  if (count == 0) return;

  ezgrpc2_message *req_msg = ezgrpc2_messages_pop(event->message.messages);
  Helloworld__HelloRequest *req = EZGRPC2_PB_UNPACK(
      Helloworld__HelloRequest, &helloworld__hello_request__descriptor, req_msg);
  ezgrpc2_message_free(req_msg);

  if (req == nullptr) {
    ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_INTERNAL);
    return;
  }

  const char *name = (req->name && req->name[0] != '\0') ? req->name : "World";
  if (!g_quiet) printf("[server] Received SayHelloAgain request for name='%s'\n", name);

  char reply_buf[256];
  snprintf(reply_buf, sizeof(reply_buf), "Hello again, %s!", name);

  Helloworld__HelloReply reply = HELLOWORLD__HELLO_REPLY__INIT;
  reply.message = reply_buf;

  ezgrpc2_messages *res_msgs = ezgrpc2_pb_create_response((ProtobufCMessage *)&reply);
  EZGRPC2_PB_FREE_UNPACKED(req);

  ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, res_msgs);
  ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_OK);
  ezgrpc2_messages_free(res_msgs);
}

// 3. SayHelloStream (Server Streaming: 1 request -> 3 responses)
static void handle_say_hello_stream(ezgrpc2_server *server, ezgrpc2_event *event) {
  size_t count = ezgrpc2_messages_count(event->message.messages);
  if (count == 0) return;

  ezgrpc2_message *req_msg = ezgrpc2_messages_pop(event->message.messages);
  Helloworld__HelloRequest *req = EZGRPC2_PB_UNPACK(
      Helloworld__HelloRequest, &helloworld__hello_request__descriptor, req_msg);
  ezgrpc2_message_free(req_msg);

  if (req == nullptr) {
    ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_INTERNAL);
    return;
  }

  const char *name = (req->name && req->name[0] != '\0') ? req->name : "World";
  if (!g_quiet) printf("[server] Received SayHelloStream request for name='%s'\n", name);

  ezgrpc2_messages *res_msgs = ezgrpc2_messages_new(nullptr);
  for (int i = 1; i <= 3; i++) {
    char reply_buf[256];
    snprintf(reply_buf, sizeof(reply_buf), "Stream message #%d for %s", i, name);

    Helloworld__HelloReply reply = HELLOWORLD__HELLO_REPLY__INIT;
    reply.message = reply_buf;

    size_t packed_len = protobuf_c_message_get_packed_size((ProtobufCMessage *)&reply);
    uint8_t *buf = malloc(packed_len);
    protobuf_c_message_pack((ProtobufCMessage *)&reply, buf);
    ezgrpc2_message *msg = ezgrpc2_message_new(0, buf, packed_len);
    free(buf);
    ezgrpc2_messages_push(res_msgs, msg);
  }

  EZGRPC2_PB_FREE_UNPACKED(req);
  ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, res_msgs);
  ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_OK);
  ezgrpc2_messages_free(res_msgs);
}

// 4. SayHelloClientStream (Client Streaming: N requests -> 1 response)
static void handle_say_hello_client_stream(ezgrpc2_server *server, ezgrpc2_event *event) {
  client_stream_state_t *st = get_or_create_client_stream(event->message.stream_id);

  while (ezgrpc2_messages_count(event->message.messages) > 0) {
    ezgrpc2_message *req_msg = ezgrpc2_messages_pop(event->message.messages);
    Helloworld__HelloRequest *req = EZGRPC2_PB_UNPACK(
        Helloworld__HelloRequest, &helloworld__hello_request__descriptor, req_msg);
    ezgrpc2_message_free(req_msg);

    if (req != nullptr && st != nullptr && st->count < MAX_CLIENT_STREAM_REQS) {
      const char *name = (req->name && req->name[0] != '\0') ? req->name : "Friend";
      strncpy(st->names[st->count], name, sizeof(st->names[st->count]) - 1);
      st->names[st->count][sizeof(st->names[st->count]) - 1] = '\0';
      st->count++;
      EZGRPC2_PB_FREE_UNPACKED(req);
    }
  }

  if (event->message.end_stream) {
    char summary_buf[512] = "Hello to all: ";
    size_t total = st ? st->count : 0;
    for (size_t i = 0; i < total; i++) {
      if (i > 0) strncat(summary_buf, ", ", sizeof(summary_buf) - strlen(summary_buf) - 1);
      strncat(summary_buf, st->names[i], sizeof(summary_buf) - strlen(summary_buf) - 1);
    }
    char count_str[64];
    snprintf(count_str, sizeof(count_str), " (Total: %zu)", total);
    strncat(summary_buf, count_str, sizeof(summary_buf) - strlen(summary_buf) - 1);

    if (!g_quiet) printf("[server] SayHelloClientStream complete. Summary: '%s'\n", summary_buf);

    Helloworld__HelloReply reply = HELLOWORLD__HELLO_REPLY__INIT;
    reply.message = summary_buf;
    ezgrpc2_messages *res_msgs = ezgrpc2_pb_create_response((ProtobufCMessage *)&reply);

    ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, res_msgs);
    ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_OK);
    ezgrpc2_messages_free(res_msgs);

    remove_client_stream(event->message.stream_id);
  }
}

// 5. SayHelloBidiStream (Bidirectional Streaming: duplex interactive stream)
static void handle_say_hello_bidi_stream(ezgrpc2_server *server, ezgrpc2_event *event) {
  while (ezgrpc2_messages_count(event->message.messages) > 0) {
    ezgrpc2_message *req_msg = ezgrpc2_messages_pop(event->message.messages);
    Helloworld__HelloRequest *req = EZGRPC2_PB_UNPACK(
        Helloworld__HelloRequest, &helloworld__hello_request__descriptor, req_msg);
    ezgrpc2_message_free(req_msg);

    if (req != nullptr) {
      const char *name = (req->name && req->name[0] != '\0') ? req->name : "World";
      if (!g_quiet) printf("[server] Received SayHelloBidiStream request for name='%s'\n", name);

      char reply_buf[256];
      snprintf(reply_buf, sizeof(reply_buf), "Bidi reply to: %s", name);

      Helloworld__HelloReply reply = HELLOWORLD__HELLO_REPLY__INIT;
      reply.message = reply_buf;
      ezgrpc2_messages *res_msgs = ezgrpc2_pb_create_response((ProtobufCMessage *)&reply);
      ezgrpc2_server_session_stream_send(server, event->session_uuid, event->message.stream_id, res_msgs);
      ezgrpc2_messages_free(res_msgs);
      EZGRPC2_PB_FREE_UNPACKED(req);
    }
  }

  if (event->message.end_stream) {
    if (!g_quiet) printf("[server] SayHelloBidiStream client closed stream %d\n", event->message.stream_id);
    ezgrpc2_server_session_stream_end(server, event->session_uuid, event->message.stream_id, EZGRPC2_GRPC_STATUS_OK);
  }
}

// 6. SayHelloWithError (Rich Error Model Simulation)
static void handle_say_hello_with_error(ezgrpc2_server *server, ezgrpc2_event *event) {
  // Free incoming request message if any
  while (ezgrpc2_messages_count(event->message.messages) > 0) {
    ezgrpc2_message *m = ezgrpc2_messages_pop(event->message.messages);
    ezgrpc2_message_free(m);
  }

  // Pack rich error detail: google.rpc.ErrorInfo Any payload
  // Protobuf wire encoding of ErrorInfo { reason = "INVALID_NAME", domain = "helloworld" }
  const uint8_t error_info_payload[] = {
      0x0a, 0x0c, 'I', 'N', 'V', 'A', 'L', 'I', 'D', '_', 'N', 'A', 'M', 'E',
      0x12, 0x0a, 'h', 'e', 'l', 'l', 'o', 'w', 'o', 'r', 'l', 'd',
  };

  ezgrpc2_status_detail_entry detail = {
      .type_url = "type.googleapis.com/google.rpc.ErrorInfo",
      .value = error_info_payload,
      .value_len = sizeof(error_info_payload),
  };

  if (!g_quiet) printf("[server] Ending stream with rich status details...\n");

  ezgrpc2_server_session_stream_end_with_status_details(
      server, event->session_uuid, event->message.stream_id,
      EZGRPC2_GRPC_STATUS_INVALID_ARGUMENT,
      "Name validation failed: name cannot be empty",
      &detail, 1,
      nullptr, 0);
}

// Interceptor hook: simple request observer
static ezgrpc2_grpc_status logging_pre_hook(
    [[maybe_unused]] ezgrpc2_server *server,
    const ezgrpc2_interceptor_info *info,
    [[maybe_unused]] const char **custom_error_msg,
    [[maybe_unused]] void *interceptor_data) {
  if (!g_quiet && info->path) {
    printf("[interceptor] Pre-call hook for path='%s' stream_id=%d\n", info->path, info->stream_id);
  }
  return EZGRPC2_GRPC_STATUS_OK;
}

static void handle_events(ezgrpc2_server *server, ezgrpc2_events *events) {
  ezgrpc2_event *event;
  size_t nb_event;

  while ((event = ezgrpc2_events_read(events, &nb_event)) != nullptr) {
    for (size_t i = 0; i < nb_event; i++) {
      if (ezgrpc2_health_handle_event(server, &event[i])) {
        continue;
      }
      if (ezgrpc2_reflection_handle_event(server, &event[i])) {
        continue;
      }

      switch (event[i].type) {
      case EZGRPC2_EVENT_MESSAGE: {
        method_handler_t *handler = event[i].userdata;
        if (handler != nullptr && handler->handler != nullptr) {
          if (!ezgrpc2_server_intercept_pre(server, &event[i], handler->name)) {
            break;
          }
          handler->handler(server, &event[i]);
        }
        break;
      }
      case EZGRPC2_EVENT_DATALOSS:
        ezgrpc2_server_session_stream_end(
            server, event[i].session_uuid, event[i].dataloss.stream_id,
            EZGRPC2_GRPC_STATUS_DATA_LOSS);
        break;
      case EZGRPC2_EVENT_TIMEOUT:
        if (!g_quiet) printf("[server] Stream %d timed out (deadline exceeded)\n", event[i].timeout.stream_id);
        break;
      case EZGRPC2_EVENT_CANCEL:
        if (!g_quiet) printf("[server] Stream %d cancelled by client\n", event[i].cancel.stream_id);
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

  method_handler_t say_hello_handler = {"SayHello", handle_say_hello};
  method_handler_t say_hello_again_handler = {"SayHelloAgain", handle_say_hello_again};
  method_handler_t say_hello_stream_handler = {"SayHelloStream", handle_say_hello_stream};
  method_handler_t say_hello_client_stream_handler = {"SayHelloClientStream", handle_say_hello_client_stream};
  method_handler_t say_hello_bidi_stream_handler = {"SayHelloBidiStream", handle_say_hello_bidi_stream};
  method_handler_t say_hello_error_handler = {"SayHelloWithError", handle_say_hello_with_error};

  ezgrpc2_server *server = ezgrpc2_server_new("0.0.0.0", port, "::", port, 16, nullptr, nullptr);
  if (server == nullptr) {
    fprintf(stderr, "Failed to create ezgrpc2 server on port %d\n", port);
    return 1;
  }

  printf("[server] Greeter gRPC server listening on port %d (IPv4 & IPv6)...\n", port);

  /* Register service methods */
  (void)ezgrpc2_server_register_path(server, "/helloworld.Greeter/SayHello", &say_hello_handler, 0, 0);
  (void)ezgrpc2_server_register_path(server, "/helloworld.Greeter/SayHelloAgain", &say_hello_again_handler, 0, 0);
  (void)ezgrpc2_server_register_path(server, "/helloworld.Greeter/SayHelloStream", &say_hello_stream_handler, 0, 0);
  (void)ezgrpc2_server_register_path(server, "/helloworld.Greeter/SayHelloClientStream", &say_hello_client_stream_handler, 0, 0);
  (void)ezgrpc2_server_register_path(server, "/helloworld.Greeter/SayHelloBidiStream", &say_hello_bidi_stream_handler, 0, 0);
  (void)ezgrpc2_server_register_path(server, "/helloworld.Greeter/SayHelloWithError", &say_hello_error_handler, 0, 0);

  /* Register gRPC Health Checking Protocol */
  (void)ezgrpc2_health_register(server);

  /* Register gRPC Server Reflection Protocol */
  (void)ezgrpc2_reflection_register(server);

  /* Register request interceptor */
  ezgrpc2_interceptor inc = {
      .name = "logging_interceptor",
      .pre_fn = logging_pre_hook,
      .post_fn = nullptr,
      .user_data = nullptr,
  };
  ezgrpc2_server_add_interceptor(server, &inc);

  ezgrpc2_events *events = ezgrpc2_events_new(nullptr);

  while (g_running) {
    int res = ezgrpc2_server_poll(server, events, 500);
    if (res >= 0) {
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
