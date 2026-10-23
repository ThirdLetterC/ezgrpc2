#ifndef EZGRPC2_TIMEOUT_H
#define EZGRPC2_TIMEOUT_H

#include <stdint.h>
#include <stddef.h>
#include "defs.h"

/**
 * Returns current monotonic time in milliseconds.
 */
EZGRPC2_API uint64_t ezgrpc2_time_now_ms(void);

/**
 * Parses a gRPC timeout string according to the specification:
 *   Timeout = PositiveInt TimeoutUnit
 *   TimeoutUnit = "H" / "M" / "S" / "m" / "u" / "n"
 *
 * Examples:
 *   "10S"  -> 10000 ms
 *   "500m" -> 500 ms
 *   "2M"   -> 120000 ms
 *   "1H"   -> 3600000 ms
 *
 * Returns duration in milliseconds, or -1 on invalid format / overflow.
 */
EZGRPC2_API int64_t ezgrpc2_timeout_parse_ms(const char *timeout_str);

struct ezgrpc2_server;
/**
 * Checks all active streams across all sessions for deadline expiration.
 * For expired streams, posts EZGRPC2_EVENT_TIMEOUT and ends the stream
 * with EZGRPC2_GRPC_STATUS_DEADLINE_EXCEEDED.
 */
EZGRPC2_API void ezgrpc2_server_check_deadlines(struct ezgrpc2_server *server);

#endif /* EZGRPC2_TIMEOUT_H */
