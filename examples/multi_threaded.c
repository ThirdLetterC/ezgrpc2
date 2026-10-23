/* The author disclaims copyright to this source code
 * and releases it into the public domain.
 *
 * Handlers that answer from worker threads: the handler takes a reference to
 * the call and queues it; a worker computes the reply and sends it with the
 * thread-safe ezgrpc2_call_*_async() functions, which wake the server up.
 *
 *   ./multi_threaded.bin [port] [workers]
 *
 * Method (raw bytes, no protobuf):
 *   /example.Work/Reverse  unary: replies with the request reversed
 */
#include <pthread.h>
#include <signal.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>

#include "ezgrpc2.h"

static ezgrpc2_server *g_server;

static void on_signal([[maybe_unused]] int sig) {
  /* safe in a signal handler */
  ezgrpc2_server_stop(g_server);
}

/* a minimal work queue of calls */
typedef struct job job;
struct job {
  ezgrpc2_call *call;
  job *next;
};

static pthread_mutex_t queue_lock = PTHREAD_MUTEX_INITIALIZER;
static pthread_cond_t queue_cond = PTHREAD_COND_INITIALIZER;
static job *queue_head, *queue_tail;
static bool queue_closed;

static void queue_push(ezgrpc2_call *call) {
  job *j = malloc(sizeof(*j));
  if (j == nullptr) {
    ezgrpc2_call_finish(call, EZGRPC2_STATUS_RESOURCE_EXHAUSTED, nullptr);
    ezgrpc2_call_unref(call);
    return;
  }
  *j = (job){.call = call};
  pthread_mutex_lock(&queue_lock);
  if (queue_tail != nullptr)
    queue_tail->next = j;
  else
    queue_head = j;
  queue_tail = j;
  pthread_cond_signal(&queue_cond);
  pthread_mutex_unlock(&queue_lock);
}

static ezgrpc2_call *queue_pop(void) {
  pthread_mutex_lock(&queue_lock);
  while (queue_head == nullptr && !queue_closed)
    pthread_cond_wait(&queue_cond, &queue_lock);
  job *j = queue_head;
  if (j != nullptr) {
    queue_head = j->next;
    if (queue_head == nullptr)
      queue_tail = nullptr;
  }
  pthread_mutex_unlock(&queue_lock);
  if (j == nullptr)
    return nullptr;
  ezgrpc2_call *call = j->call;
  free(j);
  return call;
}

static void *worker(void *arg) {
  (void)arg;
  ezgrpc2_call *call;
  while ((call = queue_pop()) != nullptr) {
    /* the request stays valid as long as the call */
    const ezgrpc2_message *request = ezgrpc2_call_request(call);
    ezgrpc2_message *reply = ezgrpc2_message_alloc(request->len);
    if (reply != nullptr) {
      for (uint32_t i = 0; i < request->len; i++)
        reply->data[i] = request->data[request->len - 1 - i];
      ezgrpc2_call_send_async(call, reply);
      ezgrpc2_call_finish_async(call, EZGRPC2_STATUS_OK, nullptr);
    } else {
      ezgrpc2_call_finish_async(call, EZGRPC2_STATUS_RESOURCE_EXHAUSTED, nullptr);
    }
    ezgrpc2_call_unref(call);
  }
  return nullptr;
}

/* runs on the server thread: hand the call to a worker */
static void reverse(ezgrpc2_call *call, [[maybe_unused]] const ezgrpc2_message *request,
                    [[maybe_unused]] void *userdata) {
  queue_push(ezgrpc2_call_ref(call));
}

int main(int argc, char **argv) {
  int port = argc > 1 ? atoi(argv[1]) : 19009;
  int nb_workers = argc > 2 ? atoi(argv[2]) : 4;
  if (nb_workers < 1 || nb_workers > 64)
    nb_workers = 4;

  signal(SIGPIPE, SIG_IGN);

  g_server = ezgrpc2_server_new(&(ezgrpc2_config){.port = (uint16_t)port});
  if (g_server == nullptr)
    return 1;
  ezgrpc2_server_handle(g_server, "/example.Work/Reverse", reverse, nullptr);

  signal(SIGINT, on_signal);
  signal(SIGTERM, on_signal);

  pthread_t workers[64];
  for (int i = 0; i < nb_workers; i++)
    pthread_create(&workers[i], nullptr, worker, nullptr);

  ezgrpc2_server_run(g_server);

  /* drain the workers before freeing the server: they hold call references */
  pthread_mutex_lock(&queue_lock);
  queue_closed = true;
  pthread_cond_broadcast(&queue_cond);
  pthread_mutex_unlock(&queue_lock);
  for (int i = 0; i < nb_workers; i++)
    pthread_join(workers[i], nullptr);

  ezgrpc2_server_free(g_server);
  return 0;
}
