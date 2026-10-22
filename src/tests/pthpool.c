#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include <unistd.h>
#include "ezgrpc2_list.h"
#include "ezgrpc2_pthpool.h"

void ret_cleanup(void *p) {
  puts("called ret_cleanup");
  free(p);
}

void usrdata_cleanup(void *p) {
  puts("called usrdata_cleanup");
  free(p);
}

void *callback([[maybe_unused]] void *data) {
  return nullptr;
}

int main(void) {
  ezgrpc2_pthpool *pool = ezgrpc2_pthpool_new(4, 0);
  ezgrpc2_list *l = ezgrpc2_list_new(nullptr);

  for (int i = 0; i < 1000; i++) {
    ezgrpc2_pthpool_add_task(pool, callback, nullptr, nullptr, nullptr);
    ezgrpc2_pthpool_poll(pool, l);
    ezgrpc2_pthpool_result_t *t;
    while ((t = ezgrpc2_list_pop_front(l)) != nullptr) {
      free(t);
    }
  }

  // Poll any remaining tasks
  sleep(1);
  ezgrpc2_pthpool_poll(pool, l);
  ezgrpc2_pthpool_result_t *t;
  while ((t = ezgrpc2_list_pop_front(l)) != nullptr) {
    free(t);
  }

  ezgrpc2_list_free(l);
  ezgrpc2_pthpool_free(pool);
  return 0;
}
