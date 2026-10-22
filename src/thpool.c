/* thpool.c - A simple thread pool */
#include <stdio.h>
#include <stdlib.h>
#include <sched.h>
#include "thpool.h"
#include "thpool_struct.h"

#define force_assert(s)                                                        \
  do {                                                                         \
    if (!(s)) {                                                                \
      fprintf(stderr, "%s:%d @ %s: assertion failed: " #s "\n", __FILE__,      \
              __LINE__, __func__);                                             \
      abort();                                                                 \
    }                                                                          \
  } while (0)

typedef struct task_t task_t;
struct task_t {
  void (*userdata_cleanup)(void *userdata);
  void (*func)(void *userdata);
  void *userdata;
};

static void *worker(void *arg) {
  thpool *pool = arg;

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

    force_assert(!pthread_mutex_unlock(&pool->mutex));
    if (task != nullptr) {
      task->func(task->userdata);
      free(task);
    }
  }

  force_assert(!pthread_mutex_lock(&pool->mutex));
  pool->live--;
  force_assert(!pthread_mutex_unlock(&pool->mutex));
  return nullptr;
}

int thpool_init(thpool *pool, int workers, [[maybe_unused]] int flags) {
  if (pool == nullptr || workers <= 0)
    return 1;
  pthread_mutex_init(&pool->mutex, nullptr);
  pthread_cond_init(&pool->wcond, nullptr);

  pool->queue = ezgrpc2_list_new(nullptr);
  pool->live = 0;
  pool->nb_threads = workers;
  pool->stop = 0;

  pthread_t *threads = malloc(sizeof(*threads) * workers);
  if (threads == nullptr)
    return 1;
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
  return 0;
}

thpool *thpool_new(int workers, int flags) {
  thpool *pool = malloc(sizeof(*pool));
  if (pool == nullptr)
    return nullptr;
  if (thpool_init(pool, workers, flags)) {
    free(pool);
    return nullptr;
  }
  return pool;
}

int thpool_add_task(thpool *pool, void (*func)(void*), void *userdata, void (*userdata_cleanup)(void*)) {
  force_assert(!pthread_mutex_lock(&pool->mutex));
  int c = 0;

  task_t *task = malloc(sizeof(*task));
  if (task == nullptr) {
    c = 1;
    goto unlock;
  }
  task->func = func;
  task->userdata = userdata;
  task->userdata_cleanup = userdata_cleanup;

  if (ezgrpc2_list_push_back(pool->queue, task)) {
    c = 1;
    free(task);
    goto unlock;
  }

  force_assert(!pthread_cond_broadcast(&pool->wcond));

unlock:
  force_assert(!pthread_mutex_unlock(&pool->mutex));
  return c;
}

void thpool_stop_and_join(thpool *pool) {
  force_assert(!pthread_mutex_lock(&pool->mutex));
  pool->stop = 1;
  force_assert(!pthread_cond_broadcast(&pool->wcond));
  force_assert(!pthread_mutex_unlock(&pool->mutex));
  for (int i = 0; i < pool->nb_threads; i++) 
    pthread_join(pool->threads[i], nullptr);
}

void thpool_cleanup(thpool *pool) {
  if (pool == nullptr) return;
  thpool_stop_and_join(pool);

  task_t *d;
  while ((d = ezgrpc2_list_pop_front(pool->queue)) != nullptr) {
    if (d->userdata_cleanup != nullptr)
      d->userdata_cleanup(d->userdata);
    free(d);
  }

  ezgrpc2_list_free(pool->queue);
  free(pool->threads);
  pthread_mutex_destroy(&pool->mutex);
  pthread_cond_destroy(&pool->wcond);
}

void thpool_free(thpool *pool) {
  if (pool == nullptr) return;
  thpool_cleanup(pool);
  free(pool);
}
