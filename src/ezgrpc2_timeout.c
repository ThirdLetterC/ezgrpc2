#include <time.h>
#include <ctype.h>
#include <stdlib.h>
#include <limits.h>
#include "core.h"
#include "ezgrpc2_session.h"
#include "ezgrpc2_events_struct.h"
#include "ezgrpc2_list_struct.h"
#include "ezgrpc2_timeout.h"

EZGRPC2_API uint64_t ezgrpc2_time_now_ms(void) {
  struct timespec ts;
  clock_gettime(CLOCK_MONOTONIC, &ts);
  return (uint64_t)ts.tv_sec * 1000ULL + (uint64_t)ts.tv_nsec / 1000000ULL;
}

EZGRPC2_API int64_t ezgrpc2_timeout_parse_ms(const char *timeout_str) {
  if (timeout_str == nullptr || *timeout_str == '\0') {
    return -1;
  }

  // Find where numeric value ends
  const char *p = timeout_str;
  while (*p && isdigit((unsigned char)*p)) {
    p++;
  }

  if (p == timeout_str || *p == '\0') {
    return -1; // No digits or missing unit
  }

  char unit = *p;
  // If there are extra characters after the unit
  if (*(p + 1) != '\0') {
    return -1;
  }

  // Parse numeric part
  char *endptr = nullptr;
  unsigned long long val = strtoull(timeout_str, &endptr, 10);
  if (endptr != p || val == 0) {
    return -1;
  }

  uint64_t ms = 0;
  switch (unit) {
  case 'H': // Hour
    if (val > (ULLONG_MAX / (3600ULL * 1000ULL))) return -1;
    ms = val * 3600ULL * 1000ULL;
    break;
  case 'M': // Minute
    if (val > (ULLONG_MAX / (60ULL * 1000ULL))) return -1;
    ms = val * 60ULL * 1000ULL;
    break;
  case 'S': // Second
    if (val > (ULLONG_MAX / 1000ULL)) return -1;
    ms = val * 1000ULL;
    break;
  case 'm': // Millisecond
    ms = val;
    break;
  case 'u': // Microsecond
    ms = (val + 999ULL) / 1000ULL; // Ceiling to at least 1ms if non-zero
    break;
  case 'n': // Nanosecond
    ms = (val + 999999ULL) / 1000000ULL; // Ceiling to at least 1ms if non-zero
    break;
  default:
    return -1;
  }

  if (ms > (uint64_t)INT64_MAX) {
    return INT64_MAX;
  }

  return (int64_t)ms;
}

EZGRPC2_API void ezgrpc2_server_check_deadlines(ezgrpc2_server *server) {
  if (server == nullptr || server->sessions == nullptr) {
    return;
  }
  uint64_t now_ms = ezgrpc2_time_now_ms();

  for (size_t i = 2; i < server->nb_sessions; i++) {
    ezgrpc2_session *session = &server->sessions[i];
    if (session->ngsession == nullptr || session->lstreams == nullptr) {
      continue;
    }

    i32 expired_sids[64];
    void *userdatas[64];
    size_t count = 0;

    eznode_t *node = session->lstreams->front;
    while (node != nullptr && count < 64) {
      ezgrpc2_stream *stream = (ezgrpc2_stream *)node->data;
      if (stream != nullptr && stream->is_deadline_set && now_ms >= stream->deadline_ms) {
        stream->is_deadline_set = 0;
        expired_sids[count] = stream->stream_id;
        userdatas[count] = stream->path ? stream->path->userdata : nullptr;
        count++;
      }
      node = node->next;
    }

    for (size_t s = 0; s < count; s++) {
      if (server->events != nullptr) {
        ezgrpc2_event ev = {
          .type = EZGRPC2_EVENT_TIMEOUT,
          .session_uuid = session->session_uuid,
          .userdata = userdatas[s],
          .timeout = {
            .stream_id = expired_sids[s],
          }
        };
        ezgrpc2_events_write(server->events, ev);
      }
      ezgrpc2_server_session_stream_end2(server, session->session_uuid, expired_sids[s],
                                         EZGRPC2_GRPC_STATUS_DEADLINE_EXCEEDED,
                                         "Deadline Exceeded", nullptr, 0);
    }
  }
}

