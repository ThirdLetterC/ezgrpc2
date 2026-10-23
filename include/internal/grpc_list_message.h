#pragma once

#include "grpc_defs.h"
#include "grpc_list.h"
#include "grpc_message.h"

typedef struct grpc_list_message_t grpc_list_message_t;

static inline grpc_list_message_t *grpc_list_message_new([[maybe_unused]] void *unused) {
  return (grpc_list_message_t *)grpc_list_new();
}

static inline void grpc_list_message_free(grpc_list_message_t *list) {
  if (list == nullptr)
    return;
  grpc_list *l = (grpc_list *)list;
  grpc_message *msg;
  while ((msg = (grpc_message *)grpc_list_pop_front(l)) != nullptr) {
    grpc_message_free(msg);
  }
  grpc_list_free(l);
}

static inline int grpc_list_message_push_back(grpc_list_message_t *list,
                                                 grpc_message *message) {
  return grpc_list_push_back((grpc_list *)list, (void *)message);
}

static inline int grpc_list_message_push_front(grpc_list_message_t *list,
                                                  grpc_message *message) {
  return grpc_list_push_front((grpc_list *)list, (void *)message);
}

static inline grpc_message *grpc_list_message_pop_front(grpc_list_message_t *list) {
  return (grpc_message *)grpc_list_pop_front((grpc_list *)list);
}

static inline grpc_message *grpc_list_message_pop_back(grpc_list_message_t *list) {
  return (grpc_message *)grpc_list_pop_back((grpc_list *)list);
}

static inline grpc_message *grpc_list_message_peek_front(grpc_list_message_t *list) {
  return (grpc_message *)grpc_list_peek_front((grpc_list *)list);
}

static inline grpc_message *grpc_list_message_peek_back(grpc_list_message_t *list) {
  return (grpc_message *)grpc_list_peek_back((grpc_list *)list);
}

static inline size_t grpc_list_message_count(grpc_list_message_t *list) {
  return grpc_list_count((grpc_list *)list);
}

static inline int grpc_list_message_is_empty(grpc_list_message_t *list) {
  return grpc_list_is_empty((grpc_list *)list);
}

static inline void grpc_list_message_concat_and_empty_src(grpc_list_message_t *dst,
                                                             grpc_list_message_t *src) {
  grpc_list_concat_and_empty_src((grpc_list *)dst, (grpc_list *)src);
}
