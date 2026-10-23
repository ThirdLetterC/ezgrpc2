#pragma once

#include <stdint.h>

struct grpc_server;

/* Return the CLOCK_MONOTONIC time in milliseconds. */
uint64_t grpc_time_now_ms(void);

/* Parse a grpc-timeout value ("100m", "5S", ...) into milliseconds; return -1 if invalid. */
int64_t grpc_timeout_parse_ms(const char *timeout_str);

/* Ensure the deadline timer fires no later than `deadline_ms`. */
void server_arm_deadline(struct grpc_server *server, uint64_t deadline_ms);
