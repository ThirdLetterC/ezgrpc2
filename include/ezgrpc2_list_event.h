#ifndef EZGRPC2_LIST_EVENT_H
#define EZGRPC2_LIST_EVENT_H

#include "defs.h"
#include "ezgrpc2_list.h"
#include "ezgrpc2_event.h"

typedef struct ezgrpc2_list_event_t ezgrpc2_list_event_t;

static inline ezgrpc2_list_event_t *ezgrpc2_list_event_new([[maybe_unused]] void *unused) {
  return (ezgrpc2_list_event_t *)ezgrpc2_list_new(nullptr);
}

static inline void ezgrpc2_list_event_free(ezgrpc2_list_event_t *list) {
  if (list == nullptr)
    return;
  ezgrpc2_list *l = (ezgrpc2_list *)list;
  ezgrpc2_event *ev;
  while ((ev = (ezgrpc2_event *)ezgrpc2_list_pop_front(l)) != nullptr) {
    if (ev->type == EZGRPC2_EVENT_MESSAGE && ev->message.messages != nullptr) {
      ezgrpc2_messages_free(ev->message.messages);
    }
    free(ev);
  }
  ezgrpc2_list_free(l);
}

static inline int ezgrpc2_list_event_push_back(ezgrpc2_list_event_t *list, ezgrpc2_event *event) {
  return ezgrpc2_list_push_back((ezgrpc2_list *)list, (void *)event);
}

static inline int ezgrpc2_list_event_push_front(ezgrpc2_list_event_t *list, ezgrpc2_event *event) {
  return ezgrpc2_list_push_front((ezgrpc2_list *)list, (void *)event);
}

static inline ezgrpc2_event *ezgrpc2_list_event_pop_front(ezgrpc2_list_event_t *list) {
  return (ezgrpc2_event *)ezgrpc2_list_pop_front((ezgrpc2_list *)list);
}

static inline ezgrpc2_event *ezgrpc2_list_event_pop_back(ezgrpc2_list_event_t *list) {
  return (ezgrpc2_event *)ezgrpc2_list_pop_back((ezgrpc2_list *)list);
}

static inline ezgrpc2_event *ezgrpc2_list_event_peek_front(ezgrpc2_list_event_t *list) {
  return (ezgrpc2_event *)ezgrpc2_list_peek_front((ezgrpc2_list *)list);
}

static inline ezgrpc2_event *ezgrpc2_list_event_peek_back(ezgrpc2_list_event_t *list) {
  return (ezgrpc2_event *)ezgrpc2_list_peek_back((ezgrpc2_list *)list);
}

static inline size_t ezgrpc2_list_event_count(ezgrpc2_list_event_t *list) {
  return ezgrpc2_list_count((ezgrpc2_list *)list);
}

static inline int ezgrpc2_list_event_is_empty(ezgrpc2_list_event_t *list) {
  return ezgrpc2_list_is_empty((ezgrpc2_list *)list);
}

static inline void ezgrpc2_list_event_concat_and_empty_src(ezgrpc2_list_event_t *dst,
                                                           ezgrpc2_list_event_t *src) {
  ezgrpc2_list_concat_and_empty_src((ezgrpc2_list *)dst, (ezgrpc2_list *)src);
}

#endif /* EZGRPC2_LIST_EVENT_H */
