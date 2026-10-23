#pragma once
#include <stdlib.h>
#include "ezgrpc2_list.h"

typedef struct eznode_t eznode_t;
struct eznode_t {
  eznode_t *next;
  eznode_t *prev;
  void *data;
};

struct ezgrpc2_list {
  /* Think of the list as front -> back. The front has no previous node, and
   * the back has no next node. */
  eznode_t *front;
  eznode_t *back;
  size_t count;
};

static inline void ezgrpc2_list_init(ezgrpc2_list *list) {
  list->front = nullptr;
  list->back = nullptr;
  list->count = 0;
}
