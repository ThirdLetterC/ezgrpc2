#ifndef EZGRPC2_MESSAGES_H
#define EZGRPC2_MESSAGES_H

#include "ezgrpc2_list_message.h"

/* ezgrpc2_messages is an alias for ezgrpc2_list_message_t */
typedef ezgrpc2_list_message_t ezgrpc2_messages;

static inline ezgrpc2_message *ezgrpc2_messages_pop(ezgrpc2_messages *messages) {
  return ezgrpc2_list_message_pop_front(messages);
}

static inline ezgrpc2_message *ezgrpc2_messages_peek_front(ezgrpc2_messages *messages) {
  return ezgrpc2_list_message_peek_front(messages);
}

static inline int ezgrpc2_messages_push(ezgrpc2_messages *messages, ezgrpc2_message *message) {
  return ezgrpc2_list_message_push_back(messages, message);
}

static inline ezgrpc2_messages *ezgrpc2_messages_new([[maybe_unused]] void *unused) {
  return ezgrpc2_list_message_new(nullptr);
}

static inline size_t ezgrpc2_messages_count(ezgrpc2_messages *messages) {
  return ezgrpc2_list_message_count(messages);
}

static inline void ezgrpc2_messages_free(ezgrpc2_messages *messages) {
  ezgrpc2_list_message_free(messages);
}

#endif /* EZGRPC2_MESSAGES_H */
