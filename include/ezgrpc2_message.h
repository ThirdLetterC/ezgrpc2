#ifndef EZGRPC2_MESSAGE_H
#define EZGRPC2_MESSAGE_H
#include "defs.h"
#include <stddef.h>
#include <stdint.h>

typedef struct ezgrpc2_message ezgrpc2_message;
/**
 * A gRPC message
 */
struct ezgrpc2_message {
  uint32_t len;
  uint8_t cflag;
  /* aligned like malloc() memory, so serialization formats that are read in
   * place (FlatBuffers) can use the payload directly */
  alignas(max_align_t) uint8_t data[];
};

EZGRPC2_API ezgrpc2_message *ezgrpc2_message_new(uint8_t compressed_flag, void *data,
                                                 size_t data_len);
ezgrpc2_message *ezgrpc2_message_new2(size_t data_len);
EZGRPC2_API void ezgrpc2_message_free(ezgrpc2_message *message);
#endif
