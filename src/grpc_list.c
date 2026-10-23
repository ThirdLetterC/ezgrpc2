/* list.c - doubly linked list */

#include <stdlib.h>
#include <assert.h>
#include "core.h"
#include "grpc_list.h"
#include "grpc_list_struct.h"

grpc_list *grpc_list_new(void) {
  grpc_list *ezlist = grpc_malloc(sizeof(*ezlist));
  if (ezlist == nullptr)
    return nullptr;
  ezlist->front = nullptr;
  ezlist->back = nullptr;
  ezlist->count = 0;
  return ezlist;
}

void grpc_list_free(grpc_list *ezlist) {
  if (ezlist == nullptr)
    return;
  eznode_t *curr = ezlist->front;
  while (curr != nullptr) {
    eznode_t *next = curr->next;
    grpc_free(curr);
    curr = next;
  }
  grpc_free(ezlist);
}

/* appends to the last element */
int grpc_list_push_back(grpc_list *ezlist, void *data) {
  assert(ezlist != nullptr);
  eznode_t *new_node = grpc_malloc(sizeof(*new_node));
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
int grpc_list_push_front(grpc_list *ezlist, void *data) {
  assert(ezlist != nullptr);
  eznode_t *new_node = grpc_malloc(sizeof(*new_node));
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
void *grpc_list_pop_back(grpc_list *ezlist) {
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

  grpc_free(back);
  ezlist->count--;
  return data;
}

/* pop front */
void *grpc_list_pop_front(grpc_list *ezlist) {
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

  grpc_free(front);
  ezlist->count--;
  return data;
}

size_t grpc_list_count(grpc_list *ezlist) {
  return ezlist->count;
}

int grpc_list_is_empty(grpc_list *ezlist) {
  return !ezlist->count;
}

void *grpc_list_find(grpc_list *ezlist,
                                    int (*cmp)(const void *data, const void *userdata),
                                    void *userdata) {
  eznode_t *node = ezlist->front;
  while (node != nullptr && cmp(node->data, userdata)) {
    node = node->next;
  }

  return node != nullptr ? node->data : nullptr;
}

/* remove */
void *grpc_list_remove(grpc_list *ezlist,
                                      int (*cmp)(const void *data, const void *userdata),
                                      void *userdata) {
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
    grpc_free(node);
    ezlist->count--;
  }

  return data;
}

void *grpc_list_peek_front(grpc_list *ezlist) {
  assert(ezlist != nullptr);
  return ezlist->front != nullptr ? ezlist->front->data : nullptr;
}

void *grpc_list_peek_back(grpc_list *ezlist) {
  assert(ezlist != nullptr);
  return ezlist->back != nullptr ? ezlist->back->data : nullptr;
}

void grpc_list_concat_and_empty_src(grpc_list *dst, grpc_list *src) {
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

void grpc_list_foreach(grpc_list *ezlist,
                                      void (*func)(const void *data, const void *userdata),
                                      void *userdata) {
  for (eznode_t *n = ezlist->front; n != nullptr; n = n->next)
    func(n->data, userdata);
}
