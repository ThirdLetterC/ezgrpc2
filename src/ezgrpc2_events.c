#include <assert.h>
#include <stdlib.h>
#include "ezgrpc2_events.h"
#include "ezgrpc2_mem.h"
#include "ezgrpc2_events_struct.h"

#define EZGRPC2_EVENTS_BLOCK (64)

EZGRPC2_API ezgrpc2_events *ezgrpc2_events_new([[maybe_unused]] void *unused) {
  ezgrpc2_events *events = malloc(sizeof(*events));
  if (events == nullptr)
    return nullptr;
  ezgrpc2_list_init(&events->bevents);
  events->windex = EZGRPC2_EVENTS_BLOCK;
  return events;
}

int ezgrpc2_events_write(ezgrpc2_events *events, ezgrpc2_event event) {
  if (unlikely(events->windex == EZGRPC2_EVENTS_BLOCK)) {
    ezgrpc2_event *arrevents = malloc(sizeof(*arrevents) * EZGRPC2_EVENTS_BLOCK);
    if (arrevents == nullptr) return 1;
    ezgrpc2_list_push_back(&events->bevents, arrevents);
    events->windex = 0;
  }
  ezgrpc2_event *arrevents = ezgrpc2_list_peek_back(&events->bevents);
  arrevents[events->windex++] = event;
  return 0;
}

/* returns an array of event of length len */
EZGRPC2_API ezgrpc2_event *ezgrpc2_events_read(ezgrpc2_events *events, size_t *len) {
  ezgrpc2_event *arrevents = ezgrpc2_list_pop_front(&events->bevents);
  if (arrevents == nullptr) return nullptr;
  size_t count = ezgrpc2_list_count(&events->bevents);
  if (!count) {
    *len = events->windex;
    events->windex = EZGRPC2_EVENTS_BLOCK;
  } else {
    *len = EZGRPC2_EVENTS_BLOCK;
  }

  return arrevents;
}

EZGRPC2_API void ezgrpc2_events_free(ezgrpc2_events *events) {
  if (events == nullptr) return;
  size_t nb_event;
  ezgrpc2_event *arrevents;
  while ((arrevents = ezgrpc2_events_read(events, &nb_event)) != nullptr) {
    for (size_t i = 0; i < nb_event; i++) {
      if (arrevents[i].type == EZGRPC2_EVENT_MESSAGE)
        ezgrpc2_messages_free(arrevents[i].message.messages);
    }
    free(arrevents);
  }
  free(events);
}
