#pragma once

#include <stddef.h>
#include <stdint.h>
#include "grpc_call.h"
#include "grpc_defs.h"
#include "grpc_error.h"
#include "grpc_status.h"

#define GRPC_STATUS_DETAILS_TRAILER "grpc-status-details-bin"

typedef struct grpc_status_detail_entry {
  const char *type_url;
  const uint8_t *value;
  size_t value_len;
} grpc_status_detail_entry;

/**
 * Encodes a google.rpc.Status protobuf message into Base64 for the
 * 'grpc-status-details-bin' trailing metadata.
 *
 * Caller must free the returned string with grpc_free.
 * Returns nullptr on error.
 */
GRPC_API char *grpc_status_details_encode(int32_t code, const char *message,
                                                const grpc_status_detail_entry *details,
                                                size_t nb_details, size_t *out_len);

/**
 * Decodes a 'grpc-status-details-bin' base64 string back into status code,
 * message, and the first detail entry (if present).
 *
 * ``*out_message``, ``*out_type_url``, and ``*out_detail_value`` are allocated
 * with grpc_malloc() and must be freed with grpc_free().
 */
GRPC_API grpc_err grpc_status_details_decode(const char *base64_str, int32_t *out_code,
                                              char **out_message, char **out_type_url,
                                              uint8_t **out_detail_value, size_t *out_detail_len);

/**
 * Finishes a call with rich error details, sent as ``grpc-status-details-bin``
 * (the gRPC Rich Error Model). Trailers added with grpc_call_add_trailers()
 * are sent too.
 */
GRPC_API grpc_err grpc_call_finish_with_details(grpc_call *call, grpc_status status,
                                                         const char *message,
                                                         const grpc_status_detail_entry *details,
                                                         size_t nb_details);
