#pragma once

#include <stddef.h>
#include <stdint.h>
#include "ezgrpc2_call.h"
#include "ezgrpc2_defs.h"
#include "ezgrpc2_error.h"
#include "ezgrpc2_status.h"

#define EZGRPC2_STATUS_DETAILS_TRAILER "grpc-status-details-bin"

typedef struct ezgrpc2_status_detail_entry {
  const char *type_url;
  const uint8_t *value;
  size_t value_len;
} ezgrpc2_status_detail_entry;

/**
 * Encodes a google.rpc.Status protobuf message into Base64 for the
 * 'grpc-status-details-bin' trailing metadata.
 *
 * Caller must free the returned string with ezgrpc2_free.
 * Returns nullptr on error.
 */
EZGRPC2_API char *ezgrpc2_status_details_encode(int32_t code, const char *message,
                                                const ezgrpc2_status_detail_entry *details,
                                                size_t nb_details, size_t *out_len);

/**
 * Decodes a 'grpc-status-details-bin' base64 string back into status code,
 * message, and the first detail entry (if present).
 *
 * ``*out_message``, ``*out_type_url``, and ``*out_detail_value`` are allocated
 * with ezgrpc2_malloc() and must be freed with ezgrpc2_free().
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_status_details_decode(const char *base64_str, int32_t *out_code,
                                              char **out_message, char **out_type_url,
                                              uint8_t **out_detail_value, size_t *out_detail_len);

/**
 * Finishes a call with rich error details, sent as ``grpc-status-details-bin``
 * (the gRPC Rich Error Model). Trailers added with ezgrpc2_call_add_trailers()
 * are sent too.
 */
EZGRPC2_API ezgrpc2_err ezgrpc2_call_finish_with_details(ezgrpc2_call *call, ezgrpc2_status status,
                                                         const char *message,
                                                         const ezgrpc2_status_detail_entry *details,
                                                         size_t nb_details);
