#include <stdlib.h>
#include <assert.h>
#include <stddef.h>
#include <stdint.h>
#include "ezgrpc2_arena_event.h"

struct ezgrpc2_arena_event {
  /* indexes */
  size_t write;
  size_t read;
  size_t count;
  /* size of buf */
  size_t size;
  uint8_t buf[];
};

EZGRPC2_API ezgrpc2_arena_event *ezgrpc2_arena_event_new(size_t size) {
  ezgrpc2_arena_event *arena_event = malloc(sizeof(*arena_event) + size);
  if (arena_event == nullptr)
    return nullptr;
  arena_event->read = 0;
  arena_event->write = 0;
  arena_event->count = 0;
  arena_event->size = size;
  return arena_event;
}

EZGRPC2_API void ezgrpc2_arena_event_free(ezgrpc2_arena_event *arena_event) {
  free(arena_event);
}

/* appends to the last element */
EZGRPC2_API ezgrpc2_event *ezgrpc2_arena_event_malloc(ezgrpc2_arena_event *arena_event) {
  assert(arena_event != nullptr);

  /* ensure the memory we're returning is aligned */
  const size_t align = alignof(ezgrpc2_event);
  const uintptr_t ptr = (uintptr_t)(const void *)&arena_event->buf[arena_event->write];
  const size_t pad = (align - (ptr & (align - 1))) & (align - 1);
  if (arena_event->write + sizeof(ezgrpc2_event) + pad > arena_event->size)
    return nullptr;

  ezgrpc2_event *ev = (void *)&arena_event->buf[pad + arena_event->write];
  arena_event->write += pad + sizeof(ezgrpc2_event);
  arena_event->count++;

  return ev;
}

/* reads from the arena */
EZGRPC2_API ezgrpc2_event *ezgrpc2_arena_event_read(ezgrpc2_arena_event *arena_event) {
  assert(arena_event != nullptr);
  if (!arena_event->count)
    return nullptr;

  const size_t align = alignof(ezgrpc2_event);
  const uintptr_t ptr = (uintptr_t)(const void *)&arena_event->buf[arena_event->read];
  const size_t pad = (align - (ptr & (align - 1))) & (align - 1);
  ezgrpc2_event *ev = (void *)&arena_event->buf[pad + arena_event->read];
  arena_event->read += pad + sizeof(ezgrpc2_event);
  arena_event->count--;

  return ev;
}

EZGRPC2_API int ezgrpc2_arena_event_is_empty(ezgrpc2_arena_event *arena_event) {
  return !arena_event->count;
}

EZGRPC2_API size_t ezgrpc2_arena_event_count(ezgrpc2_arena_event *arena_event) {
  return arena_event->count;
}

EZGRPC2_API void ezgrpc2_arena_event_reset(ezgrpc2_arena_event *arena_event) {
  arena_event->write = 0;
  arena_event->read = 0;
  arena_event->count = 0;
}
