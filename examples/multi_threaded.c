/* The author disclaims copyright to this source code
 * and releases it into the public domain.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY KIND,
 * EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF
 * MERCHANTABILITY, FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.
 */
#include <stdio.h>
#include <string.h>
#include <stdlib.h>
#include <unistd.h>
#include <assert.h>

#include "ezgrpc2.h"
#include "ezgrpc2_pthpool.h"

#ifdef __unix__
#include <signal.h>
#include <pthread.h>
#endif

struct userdata_t {
  i32 stream_id;
  ezgrpc2_session_uuid session_uuid;
  char end_stream;
  ezgrpc2_grpc_status status;

  /* input messages. requested by the client */
  ezgrpc2_messages *imessages;

  /* output messages. our server response */
  ezgrpc2_messages *omessages;
};

struct path_userdata_t {
  char is_unary;
  void *(*callback)(void*);
};

struct userdata_t *create_userdata(ezgrpc2_session_uuid session_uuid, int32_t stream_id,
    ezgrpc2_messages *messages, char end_stream, ezgrpc2_grpc_status status) {
  struct userdata_t *data = malloc(sizeof(*data));
  data->session_uuid = session_uuid;
  data->stream_id = stream_id;
  data->imessages = messages;
  data->omessages = nullptr;
  data->end_stream = end_stream;
  data->status = status;
  return data;
}

void free_userdata(struct userdata_t *userdata) {
  ezgrpc2_messages_free(userdata->imessages);
  ezgrpc2_messages_free(userdata->omessages);
  free(userdata);
}

_Atomic int pp = 0;
/* No ezgrpc2_* functions must be called in this callback */
void *callback_path0(void *data) {
  printf("task executed path0!!\n");
  struct userdata_t *userdata = data;
  userdata->omessages = ezgrpc2_messages_new(nullptr);
  /* if client did not request properly, skip processing. */
  if (userdata->status != EZGRPC2_GRPC_STATUS_OK)
    return userdata;

  /* pretend this is our response (output messages) */
  for (int i = 0; i < 5; i++, pp++) {
    ezgrpc2_message *msg = ezgrpc2_message_new(0, (void *)"    path0!", 11);
    *(uint32_t*)(msg->data) = pp;
    ezgrpc2_messages_push(userdata->omessages, msg);
  }

  return data;
}

/* No ezgrpc2_* functions must be called in this callback */
void *callback_path1(void *data) {
  printf("task executed path1!!\n");
  struct userdata_t *userdata = data;
  userdata->omessages = ezgrpc2_messages_new(nullptr);
  /* if client did not request properly, skip processing. */
  if (userdata->status != EZGRPC2_GRPC_STATUS_OK)
    return userdata;

  /* pretend this is our response (output messages) */
  for (int i = 0; i < 5; i++, pp++) {
    ezgrpc2_message *msg = ezgrpc2_message_new(0, (void *)"    path1!", 11);
    *(uint32_t*)(msg->data) = pp;
    ezgrpc2_messages_push(userdata->omessages, msg);
  }

  /* cleanup (input messages) */
  ezgrpc2_messages_free(userdata->imessages);
  userdata->imessages = nullptr;

  return data;
}

static void handle_event_message(ezgrpc2_event *event,
                                 ezgrpc2_server *server,
                                 ezgrpc2_pthpool *opool,
                                 ezgrpc2_pthpool *upool) {
  size_t nb_msg = ezgrpc2_messages_count(event->message.messages);
  struct path_userdata_t *path_userdata = event->userdata;
  if (path_userdata->is_unary) {
    if (!event->message.end_stream || nb_msg != 1) {
      /* This is unary service, but they are sending more than one message */
      ezgrpc2_server_session_stream_end(server, event->session_uuid,
                                 event->message.stream_id,
                                 EZGRPC2_GRPC_STATUS_INVALID_ARGUMENT);
      ezgrpc2_messages_free(event->message.messages);
      return;
    }
  }

  struct userdata_t *data = create_userdata(event->session_uuid, event->message.stream_id,
                         event->message.messages, event->message.end_stream,
                         EZGRPC2_GRPC_STATUS_OK);

#if 1
  if (path_userdata->is_unary)
    ezgrpc2_pthpool_add_task(upool, path_userdata->callback, data, nullptr, nullptr);
  else
    ezgrpc2_pthpool_add_task(opool, path_userdata->callback, data, nullptr, nullptr);
#else
  ezgrpc2_session_end_session(server, event->session_uuid, 0, 0);
#endif
}

static void handle_event_dataloss(ezgrpc2_event *event,
                                  [[maybe_unused]] ezgrpc2_server *server,
                                  ezgrpc2_pthpool *opool,
                                  ezgrpc2_pthpool *upool) {
  printf("stream_id %d, event dataloss\n", event->dataloss.stream_id);
  struct path_userdata_t *path_userdata = event->userdata;
  struct userdata_t *data = create_userdata(event->session_uuid, event->dataloss.stream_id,
                         ezgrpc2_messages_new(nullptr), 1,
                         EZGRPC2_GRPC_STATUS_DATA_LOSS);

  if (path_userdata->is_unary)
    ezgrpc2_pthpool_add_task(upool, path_userdata->callback, data, nullptr, nullptr);
  else
    ezgrpc2_pthpool_add_task(opool, path_userdata->callback, data, nullptr, nullptr);
}

static void handle_events(ezgrpc2_server *server, ezgrpc2_events *events,
                          ezgrpc2_pthpool *opool, ezgrpc2_pthpool *upool) {
  ezgrpc2_event *event;
  size_t nb_event;
  /* check for events in the paths */
  while ((event = ezgrpc2_events_read(events, &nb_event)) != nullptr) {
    for (size_t i = 0; i < nb_event; i++) {
      switch (event[i].type) {
      case EZGRPC2_EVENT_MESSAGE:
        handle_event_message(&event[i], server, opool, upool);
        break;
      case EZGRPC2_EVENT_DATALOSS:
        handle_event_dataloss(&event[i], server, opool, upool);
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

static void handle_thread_pool(ezgrpc2_server *server, ezgrpc2_pthpool *pool) {
  ezgrpc2_list *lresults = ezgrpc2_list_new(nullptr);
  /* retrieve any finished tasks */
  ezgrpc2_pthpool_poll(pool, lresults);
  ezgrpc2_pthpool_result_t *result;
  while ((result = ezgrpc2_list_pop_front(lresults)) != nullptr) {
    struct userdata_t *data = result->ret;
    if (result->is_timeout) {
      free_userdata(result->userdata);
      ezgrpc2_pthpool_result_free(result);
      continue;
    }
    switch (ezgrpc2_server_session_stream_send(server, data->session_uuid, data->stream_id, data->omessages)) {
      case 0:
        if (data->end_stream)
          ezgrpc2_server_session_stream_end(server, data->session_uuid, data->stream_id, data->status);
        break;
      case 1:
        break;
      case 2:
        printf("stream id doesn't exist %d\n", data->stream_id);
        break;
      case 3:
        printf("fatal\n");
        break;
      default:
        assert(0);
    }
    ezgrpc2_pthpool_result_free(result);
    free_userdata(data);
  }
  ezgrpc2_list_free(lresults);
}

#ifdef __unix__
_Atomic int sigterm_flag = 0;
static void *signal_handler(void *data) {
  sigset_t *sigset = data;
  int sig, res;
  while (1) {
    res = sigwait(sigset, &sig);
    if (res > 1) {
      fprintf(stderr, "sigwait error\n");
      abort();
    }
    if (sig & SIGTERM) {
      sigterm_flag = 1;
      ssize_t w = write(STDERR_FILENO, "sigterm_flag set. waiting for next poll timeout\n", 48);
      (void)w;
    }
  }
}
#endif

int main(void) {
  int res;
  const size_t nb_paths = 2;
  struct path_userdata_t path_userdata[nb_paths];
  res = ezgrpc2_global_init(0);
  if (res) 
    abort();

#ifdef __unix__
  sigset_t sigset;
  sigemptyset(&sigset);
  sigaddset(&sigset, SIGTERM);
  sigaddset(&sigset, SIGPIPE);
  res = pthread_sigmask(SIG_BLOCK, &sigset, nullptr);
  if (res != 0) {
    fprintf(stderr, "pthread_sigmask failed\n");
    abort();
  }

  /* run our signal handler */
  pthread_t sig_thread;
  res = pthread_create(&sig_thread, nullptr, signal_handler, &sigset);
  if (res) {
    fprintf(stderr, "pthread_create failed\n");
    abort();
  }
#endif /* __unix__ */

  /* Tasks for unary requests */
  ezgrpc2_pthpool *unordered_pool = nullptr;
  /* Tasks for streaming requests */
  ezgrpc2_pthpool *ordered_pool = nullptr;

  unordered_pool = ezgrpc2_pthpool_new(2, 0);
  assert(unordered_pool != nullptr);

  /* worker must be one for an ordered execution */
  ordered_pool = ezgrpc2_pthpool_new(1, 0);
  assert(ordered_pool != nullptr);

  /* setup server settings */
  ezgrpc2_server_settings *server_settings = ezgrpc2_server_settings_new(nullptr);
  assert(server_settings != nullptr);
  ezgrpc2_server_settings_set_log_level(server_settings, EZGRPC2_SERVER_LOG_ALL);
  ezgrpc2_server_settings_set_log_fp(server_settings, stderr);

  /* The heart of this API */
  ezgrpc2_server *server = ezgrpc2_server_new("0.0.0.0", 19009, "::", 19009, 16, server_settings, nullptr);
  ezgrpc2_server_settings_free(server_settings);
  assert(server != nullptr);

  path_userdata[0].is_unary = 0;
  path_userdata[0].callback = callback_path0;

  path_userdata[1].is_unary = 1;
  path_userdata[1].callback = callback_path1;

  (void)ezgrpc2_server_register_path(server, "/test.yourAPI/whatever_service1", path_userdata, 0, 0);
  (void)ezgrpc2_server_register_path(server, "/test.yourAPI/whatever_service2", path_userdata + 1, 0, 0);

  ezgrpc2_events *events = ezgrpc2_events_new(nullptr);
  while (1) {
#ifdef __unix__
    if (sigterm_flag)
      break;
#endif
    int is_pool_empty = ezgrpc2_pthpool_is_empty(unordered_pool) &&
                        ezgrpc2_pthpool_is_empty(ordered_pool);

    int timeout = is_pool_empty ? 10000 : 10;
    if ((res = ezgrpc2_server_poll(server, events, timeout)) < 0)
      break;

    if (res > 0) {
      puts("incoming events");
      handle_events(server, events, ordered_pool, unordered_pool);
    } else if (res == 0) {
      printf("no event\n");
    }
    handle_thread_pool(server, ordered_pool);
    handle_thread_pool(server, unordered_pool);
  }

  if (res < 0) {
    printf("poll err\n");
  }

  puts("exiting");
  ezgrpc2_events_free(events);
  ezgrpc2_server_free(server);
  ezgrpc2_pthpool_free(ordered_pool);
  ezgrpc2_pthpool_free(unordered_pool);

  ezgrpc2_global_cleanup();
  return res;
}
