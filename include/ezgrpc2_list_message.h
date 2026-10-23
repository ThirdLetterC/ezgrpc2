#ifndef EZGRPC2_LIST_MESSAGE_H
#define EZGRPC2_LIST_MESSAGE_H

#include "defs.h"
#include "ezgrpc2_list.h"
#include "ezgrpc2_message.h"

typedef struct ezgrpc2_list_message_t ezgrpc2_list_message_t;

static inline ezgrpc2_list_message_t *ezgrpc2_list_message_new([[maybe_unused]] void *unused) {
  return (ezgrpc2_list_message_t *)ezgrpc2_list_new(nullptr);
}

static inline void ezgrpc2_list_message_free(ezgrpc2_list_message_t *list) {
  if (list == nullptr)
    return;
  ezgrpc2_list *l = (ezgrpc2_list *)list;
  ezgrpc2_message *msg;
  while ((msg = (ezgrpc2_message *)ezgrpc2_list_pop_front(l)) != nullptr) {
    ezgrpc2_message_free(msg);
  }
  ezgrpc2_list_free(l);
}

static inline int ezgrpc2_list_message_push_back(ezgrpc2_list_message_t *list,
                                                 ezgrpc2_message *message) {
  return ezgrpc2_list_push_back((ezgrpc2_list *)list, (void *)message);
}

static inline int ezgrpc2_list_message_push_front(ezgrpc2_list_message_t *list,
                                                  ezgrpc2_message *message) {
  return ezgrpc2_list_push_front((ezgrpc2_list *)list, (void *)message);
}

static inline ezgrpc2_message *ezgrpc2_list_message_pop_front(ezgrpc2_list_message_t *list) {
  return (ezgrpc2_message *)ezgrpc2_list_pop_front((ezgrpc2_list *)list);
}

static inline ezgrpc2_message *ezgrpc2_list_message_pop_back(ezgrpc2_list_message_t *list) {
  return (ezgrpc2_message *)ezgrpc2_list_pop_back((ezgrpc2_list *)list);
}

static inline ezgrpc2_message *ezgrpc2_list_message_peek_front(ezgrpc2_list_message_t *list) {
  return (ezgrpc2_message *)ezgrpc2_list_peek_front((ezgrpc2_list *)list);
}

static inline ezgrpc2_message *ezgrpc2_list_message_peek_back(ezgrpc2_list_message_t *list) {
  return (ezgrpc2_message *)ezgrpc2_list_peek_back((ezgrpc2_list *)list);
}

static inline size_t ezgrpc2_list_message_count(ezgrpc2_list_message_t *list) {
  return ezgrpc2_list_count((ezgrpc2_list *)list);
}

static inline int ezgrpc2_list_message_is_empty(ezgrpc2_list_message_t *list) {
  return ezgrpc2_list_is_empty((ezgrpc2_list *)list);
}

static inline void ezgrpc2_list_message_concat_and_empty_src(ezgrpc2_list_message_t *dst,
                                                             ezgrpc2_list_message_t *src) {
  ezgrpc2_list_concat_and_empty_src((ezgrpc2_list *)dst, (ezgrpc2_list *)src);
}

#endif /* EZGRPC2_LIST_MESSAGE_H */
