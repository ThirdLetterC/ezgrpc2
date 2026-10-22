#ifndef EZGRPC2_LIST_STRUCT_H
#define EZGRPC2_LIST_STRUCT_H
#include <stdlib.h>
#include "ezgrpc2_list.h"

typedef struct eznode_t eznode_t;
struct eznode_t {
  eznode_t *next;
  eznode_t *prev;
  void *data;
};

struct ezgrpc2_list {
  /* it's better to visualize this list as */
  /* front ---------------------> back */
  /* front->prev is always NULL and back next is always NULL */
  eznode_t *front;
  eznode_t *back;
  size_t count;
};

static inline void ezgrpc2_list_init(ezgrpc2_list *list) {
  list->front = nullptr;
  list->back = nullptr;
  list->count = 0;
}

#endif
