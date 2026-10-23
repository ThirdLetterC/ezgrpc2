#pragma once

#include "grpc_list_message.h"

/* grpc_messages aliases grpc_list_message_t. */
typedef grpc_list_message_t grpc_messages;

static inline grpc_message *grpc_messages_pop(grpc_messages *messages) {
  return grpc_list_message_pop_front(messages);
}

static inline grpc_message *grpc_messages_peek_front(grpc_messages *messages) {
  return grpc_list_message_peek_front(messages);
}

static inline int grpc_messages_push(grpc_messages *messages, grpc_message *message) {
  return grpc_list_message_push_back(messages, message);
}

static inline grpc_messages *grpc_messages_new([[maybe_unused]] void *unused) {
  return grpc_list_message_new(nullptr);
}

static inline size_t grpc_messages_count(grpc_messages *messages) {
  return grpc_list_message_count(messages);
}

static inline void grpc_messages_free(grpc_messages *messages) {
  grpc_list_message_free(messages);
}
