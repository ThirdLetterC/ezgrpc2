#ifndef EZGRPC2_STATUS_DETAILS_H
#define EZGRPC2_STATUS_DETAILS_H

#include <stddef.h>
#include <stdint.h>
#include "defs.h"
#include "ezgrpc2_grpc_status.h"
#include "ezgrpc2_metadata.h"
#include "ezgrpc2_server.h"

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
 * *out_message and *out_detail_value are allocated with ezgrpc2_malloc
 * (caller frees with ezgrpc2_free).
 * Returns 0 on success, non-zero on error.
 */
EZGRPC2_API int ezgrpc2_status_details_decode(const char *base64_str, int32_t *out_code,
                                              char **out_message, char **out_type_url,
                                              uint8_t **out_detail_value, size_t *out_detail_len);

/**
 * Ends a stream with rich error status details attached to trailing metadata
 * ('grpc-status-details-bin' per gRPC Rich Error Model spec).
 */
EZGRPC2_API int ezgrpc2_server_session_stream_end_with_status_details(
    ezgrpc2_server *server, ezgrpc2_session_uuid session_uuid, int32_t stream_id,
    ezgrpc2_grpc_status status, const char *message, const ezgrpc2_status_detail_entry *details,
    size_t nb_details, const ezgrpc2_metadata_entry *extra_trailers, size_t nb_extra_trailers);

#endif /* EZGRPC2_STATUS_DETAILS_H */
