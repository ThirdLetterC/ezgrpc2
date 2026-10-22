/* list.c - doubly linked list */

#include <stdlib.h>
#include <assert.h>
#include "core.h"
#include "ezgrpc2_list.h"
#include "ezgrpc2_list_struct.h"

EZGRPC2_API ezgrpc2_list *ezgrpc2_list_new([[maybe_unused]] void *unused) {
  ezgrpc2_list *ezlist = malloc(sizeof(*ezlist));
  if (ezlist == nullptr)
    return nullptr;
  ezlist->front = nullptr;
  ezlist->back = nullptr;
  ezlist->count = 0;
  return ezlist;
}

EZGRPC2_API void ezgrpc2_list_free(ezgrpc2_list *ezlist) {
  if (ezlist == nullptr)
    return;
  eznode_t *curr = ezlist->front;
  while (curr != nullptr) {
    eznode_t *next = curr->next;
    free(curr);
    curr = next;
  }
  free(ezlist);
}

/* appends to the last element */
EZGRPC2_API int ezgrpc2_list_push_back(ezgrpc2_list *ezlist, void *data) {
  assert(ezlist != nullptr);
  eznode_t *new_node = malloc(sizeof(*new_node));
  if (new_node == nullptr)
    return 1;
  new_node->data = data;
  new_node->next = nullptr;
  new_node->prev = nullptr;
  if (ezlist->back != nullptr) {
    ezlist->back->next = new_node;
    new_node->prev = ezlist->back;
    ezlist->back = new_node;
  } else {
    ezlist->front = new_node;
    ezlist->back = new_node;
  }

  ezlist->count++;

  return 0; 
}

/* inserts to the first element */
EZGRPC2_API int ezgrpc2_list_push_front(ezgrpc2_list *ezlist, void *data) {
  assert(ezlist != nullptr);
  eznode_t *new_node = malloc(sizeof(*new_node));
  if (new_node == nullptr)
    return 1;
  new_node->data = data;
  new_node->next = ezlist->front;
  new_node->prev = nullptr;
  if (ezlist->front != nullptr)
    ezlist->front->prev = new_node;
  else 
    ezlist->back = new_node;
  ezlist->front = new_node;

  ezlist->count++;
  return 0;
}

/* removes last element */
EZGRPC2_API void *ezgrpc2_list_pop_back(ezgrpc2_list *ezlist) {
  assert(ezlist != nullptr);
  if (!ezlist->count)
    return nullptr;

  eznode_t *back = ezlist->back;
  void *data = back->data;
  ezlist->back = back->prev;

  if (ezlist->back != nullptr) {
    ezlist->back->next = nullptr;
  } else {
    ezlist->front = nullptr;
    assert(ezlist->count == 1);
  }

  free(back);
  ezlist->count--;
  return data;
}

/* pop front */
EZGRPC2_API void *ezgrpc2_list_pop_front(ezgrpc2_list *ezlist) {
  assert(ezlist != nullptr);
  if (!ezlist->count)
    return nullptr;

  eznode_t *front = ezlist->front;
  void *data = front->data;
  ezlist->front = front->next;

  if (ezlist->front != nullptr) {
    ezlist->front->prev = nullptr;
  } else {
    ezlist->back = nullptr;
    assert(ezlist->count == 1);
  }

  free(front);
  ezlist->count--;
  return data;
}

EZGRPC2_API size_t ezgrpc2_list_count(ezgrpc2_list *ezlist) {
  return ezlist->count;
}

EZGRPC2_API int ezgrpc2_list_is_empty(ezgrpc2_list *ezlist) {
  return !ezlist->count;
}

EZGRPC2_API void *ezgrpc2_list_find(ezgrpc2_list *ezlist, int (*cmp)(const void *data, const void *userdata), void *userdata) {
  eznode_t *node = ezlist->front;
  while (node != nullptr && cmp(node->data, userdata)) {
    node = node->next;
  }

  return node != nullptr ? node->data : nullptr;
}

/* remove */
EZGRPC2_API void *ezgrpc2_list_remove(ezgrpc2_list *ezlist, int (*cmp)(const void *data, const void *userdata), void *userdata) {
  eznode_t *node = ezlist->front;
  void *data = nullptr;
  while (node != nullptr && cmp(node->data, userdata)) {
    node = node->next;
  }
  if (node != nullptr) {
    eznode_t *prev = node->prev;
    eznode_t *next = node->next;
    if (prev != nullptr) {
      prev->next = next;
    } else {
      ezlist->front = next;
    }
    if (next != nullptr) {
      next->prev = prev;
    } else {
      ezlist->back = prev;
    }
    data = node->data;
    free(node);
    ezlist->count--;
  }

  return data;
}

EZGRPC2_API void *ezgrpc2_list_peek_front(ezgrpc2_list *ezlist) {
  assert(ezlist != nullptr);
  return ezlist->front != nullptr ? ezlist->front->data: nullptr;
}

EZGRPC2_API void *ezgrpc2_list_peek_back(ezgrpc2_list *ezlist) {
  assert(ezlist != nullptr);
  return ezlist->back != nullptr ? ezlist->back->data: nullptr;
}

EZGRPC2_API void ezgrpc2_list_concat_and_empty_src(ezgrpc2_list *dst, ezgrpc2_list *src) {
  if (!src->count)
    return;

  if (dst->front == nullptr) {
    dst->front = src->front;
  }
  if (dst->back != nullptr) {
    dst->back->next = src->front;
    src->front->prev = dst->back;
  } else {
    dst->back = src->back;
  }
  dst->back = src->back;
  dst->count += src->count;

  src->front = nullptr;
  src->back = nullptr;
  src->count = 0;
}

EZGRPC2_API void ezgrpc2_list_foreach(ezgrpc2_list *ezlist, void (*func)(const void *data, const void *userdata), void *userdata) {
  for (eznode_t *n = ezlist->front; n != nullptr; n = n->next)
    func(n->data, userdata);
}
