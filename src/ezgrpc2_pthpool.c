/* pthpool.c - A pollable thread pool */
#include <stdio.h>
#include <stdlib.h>
#include <pthread.h>
#include <sched.h>
#include "core.h"

typedef struct ezgrpc2_pthpool_result_t task_t;
#include "ezgrpc2_pthpool.h"

#define force_assert(s)                                                        \
  do {                                                                         \
    if (!(s)) {                                                                \
      fprintf(stderr, "%s:%d @ %s: assertion failed: " #s "\n", __FILE__,      \
              __LINE__, __func__);                                             \
      abort();                                                                 \
    }                                                                          \
  } while (0)

struct ezgrpc2_pthpool {
  size_t max_queue, max_finished;

#ifdef NO_COUNT
  size_t nb_finished, nb_queue;
#endif
  // task queue
  ezgrpc2_list *queue;
  // finished task queue
  ezgrpc2_list *finished;

  int nb_threads;
  pthread_t *threads;

  pthread_mutex_t mutex;
  pthread_cond_t wcond;
  volatile int running;
  volatile int live;
  char stop;
};

static void *worker(void *arg) {
  ezgrpc2_pthpool *pool = arg;

  force_assert(!pthread_mutex_lock(&pool->mutex));
  pool->live++;
  force_assert(!pthread_mutex_unlock(&pool->mutex));

  while (1) {
    force_assert(!pthread_mutex_lock(&pool->mutex));
    task_t *task = nullptr;
    while (!pool->stop && (task = ezgrpc2_list_pop_front(pool->queue)) == nullptr)
      pthread_cond_wait(&pool->wcond, &pool->mutex);
    if (pool->stop) {
      if (task != nullptr)
        ezgrpc2_list_push_back(pool->queue, task);
      force_assert(!pthread_mutex_unlock(&pool->mutex));
      break;
    }

    if (task != nullptr)
      pool->running++;

    force_assert(!pthread_mutex_unlock(&pool->mutex));
    if (task != nullptr) {
      if (task->timeout && time(nullptr) > task->timeout)
        task->is_timeout = 1;
      else 
        task->ret = task->func(task->userdata);

      force_assert(!pthread_mutex_lock(&pool->mutex));
#ifdef NO_COUNT
      pool->nb_queue--;
      pool->nb_finished++;
#endif
      ezgrpc2_list_push_back(pool->finished, task);
      pool->running--;
      force_assert(!pthread_mutex_unlock(&pool->mutex));
    }
  }
  force_assert(!pthread_mutex_lock(&pool->mutex));
  pool->live--;
  force_assert(!pthread_mutex_unlock(&pool->mutex));
  return nullptr;
}

EZGRPC2_API ezgrpc2_pthpool *ezgrpc2_pthpool_new(int workers, [[maybe_unused]] int flags) {
  ezgrpc2_pthpool *pool = malloc(sizeof(*pool));
  if (pool == nullptr)
    return nullptr;
  pthread_mutex_init(&pool->mutex, nullptr);
  pthread_cond_init(&pool->wcond, nullptr);

  pool->queue = ezgrpc2_list_new(nullptr);
  pool->finished = ezgrpc2_list_new(nullptr);
#ifdef NO_COUNT
  pool->nb_finished = 0;
  pool->nb_queue = 0;
#endif
  pool->running = 0;
  pool->live = 0;
  pool->nb_threads = workers;
  pool->stop = 0;

  pthread_t *threads = malloc(sizeof(*threads) * workers);
  if (threads == nullptr) {
    ezgrpc2_list_free(pool->queue);
    ezgrpc2_list_free(pool->finished);
    pthread_mutex_destroy(&pool->mutex);
    pthread_cond_destroy(&pool->wcond);
    free(pool);
    return nullptr;
  }
  pthread_attr_t attr;
  pthread_attr_init(&attr);
  pthread_attr_setdetachstate(&attr, PTHREAD_CREATE_JOINABLE);
  for (int i = 0; i < workers; i++)
    pthread_create(threads + i, nullptr, worker, pool);
  pthread_attr_destroy(&attr);
  pool->threads = threads;

  while (pool->live != workers) {
    sched_yield();
  }
  return pool;
}

EZGRPC2_API int ezgrpc2_pthpool_is_empty(ezgrpc2_pthpool *pool) {
  force_assert(!pthread_mutex_lock(&pool->mutex));
  int ret = pool->running == 0 &&
    ezgrpc2_list_peek_front(pool->finished) == nullptr &&
    ezgrpc2_list_peek_front(pool->queue) == nullptr;

  force_assert(!pthread_mutex_unlock(&pool->mutex));
  return ret;
}

EZGRPC2_API int ezgrpc2_pthpool_add_task(ezgrpc2_pthpool *pool, void *(*func)(void*), void *userdata, void (*ret_cleanup)(void *), void (*userdata_cleanup)(void*)) {
  return ezgrpc2_pthpool_add_task2(pool, func, userdata, ret_cleanup, userdata_cleanup, 0);
}

EZGRPC2_API int ezgrpc2_pthpool_add_task2(ezgrpc2_pthpool *pool, void *(*func)(void*), void *userdata, void (*ret_cleanup)(void *), void (*userdata_cleanup)(void*), time_t timeout) {
  force_assert(!pthread_mutex_lock(&pool->mutex));
  int c = 0;

#ifdef NO_COUNT
  static size_t max_queue = 0;
  if (pool->nb_queue > SIZE_MAX - 1) {
    c = 2;
    goto unlock;
  }
#endif

  task_t *task = malloc(sizeof(*task));
  if (task == nullptr) {
    c = 1;
    goto unlock;
  }
  task->is_timeout = 0;
  task->timeout = timeout;
  task->func = func;
  task->userdata = userdata;
  task->ret = nullptr;
  task->userdata_cleanup = userdata_cleanup;
  task->ret_cleanup = ret_cleanup;

  if (ezgrpc2_list_push_back(pool->queue, task)) {
    c = 1;
    free(task);
    goto unlock;
  }
#ifdef NO_COUNT
  pool->nb_queue++;

  int p;
  if ((p = pool->nb_queue) > max_queue)
    printf("max queue %zu\n", (max_queue = p));
#endif

  force_assert(!pthread_cond_broadcast(&pool->wcond));

unlock:
  force_assert(!pthread_mutex_unlock(&pool->mutex));
  return c;
}

/* checks if there's any finished tasks in the pool.
 * returns immediately
 */
EZGRPC2_API void ezgrpc2_pthpool_poll(ezgrpc2_pthpool *pool, ezgrpc2_list *l) {
  force_assert(!pthread_mutex_lock(&pool->mutex));

  ezgrpc2_list_concat_and_empty_src(l, pool->finished);

  force_assert(!pthread_mutex_unlock(&pool->mutex));
}

EZGRPC2_API void ezgrpc2_pthpool_stop_and_join(ezgrpc2_pthpool *pool) {
  force_assert(!pthread_mutex_lock(&pool->mutex));
  pool->stop = 1;
  force_assert(!pthread_cond_broadcast(&pool->wcond));
  force_assert(!pthread_mutex_unlock(&pool->mutex));
  for (int i = 0; i < pool->nb_threads; i++) 
    pthread_join(pool->threads[i], nullptr);
}

EZGRPC2_API void ezgrpc2_pthpool_result_free(ezgrpc2_pthpool_result_t *result) {
  free(result);
}

EZGRPC2_API void ezgrpc2_pthpool_free(ezgrpc2_pthpool *pool) {
  if (pool == nullptr) return;
  ezgrpc2_pthpool_stop_and_join(pool);

  task_t *d;
  while ((d = ezgrpc2_list_pop_front(pool->finished)) != nullptr) {
    if (d->userdata_cleanup != nullptr)
      d->userdata_cleanup(d->userdata);
    if (d->ret_cleanup != nullptr)
      d->ret_cleanup(d->ret);
    free(d);
  }

  while ((d = ezgrpc2_list_pop_front(pool->queue)) != nullptr) {
    if (d->userdata_cleanup != nullptr)
      d->userdata_cleanup(d->userdata);
    free(d);
  }

  ezgrpc2_list_free(pool->queue);
  ezgrpc2_list_free(pool->finished);
  pthread_mutex_destroy(&pool->mutex);
  pthread_cond_destroy(&pool->wcond);
  free(pool->threads);
  free(pool);
}
