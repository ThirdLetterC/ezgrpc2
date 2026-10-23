#ifndef EZGRPC2_TIMEOUT_H
#define EZGRPC2_TIMEOUT_H

#include <stdint.h>

struct ezgrpc2_server;

/* CLOCK_MONOTONIC in milliseconds */
uint64_t ezgrpc2_time_now_ms(void);

/* parses a grpc-timeout value ("100m", "5S", ...) into milliseconds, -1 if invalid */
int64_t ezgrpc2_timeout_parse_ms(const char *timeout_str);

/* makes sure the deadline timer fires at `deadline_ms` at the latest */
void server_arm_deadline(struct ezgrpc2_server *server, uint64_t deadline_ms);

#endif /* EZGRPC2_TIMEOUT_H */
