#ifndef EZGRPC2_METADATA_H
#define EZGRPC2_METADATA_H

#include <stddef.h>
#include <stdint.h>
#include "defs.h"
#include "ezgrpc2_header.h"
#include "ezgrpc2_server.h"
#include "ezgrpc2_session_uuid.h"
#include "ezgrpc2_grpc_status.h"

typedef struct ezgrpc2_metadata_entry {
  const char *key;
  size_t key_len;
  const char *value;
  size_t value_len;
} ezgrpc2_metadata_entry;

/**
 * Find a request header/metadata by key (case-insensitive) on the specified stream.
 * Returns pointer to the null-terminated value string, or nullptr if not found.
 * If value_len is non-null, sets *value_len to length of value.
 */
EZGRPC2_API const char *ezgrpc2_metadata_get(ezgrpc2_server *server,
                                             ezgrpc2_session_uuid session_uuid, int32_t stream_id,
                                             const char *key, size_t *value_len);

/**
 * Returns the total number of request headers received on the specified stream.
 */
EZGRPC2_API size_t ezgrpc2_metadata_count(ezgrpc2_server *server, ezgrpc2_session_uuid session_uuid,
                                          int32_t stream_id);

/**
 * Retrieves the header at index `idx` (0 <= idx < count).
 * Returns 0 on success, non-zero if out of bounds or stream not found.
 */
EZGRPC2_API int ezgrpc2_metadata_get_at(ezgrpc2_server *server, ezgrpc2_session_uuid session_uuid,
                                        int32_t stream_id, size_t idx,
                                        ezgrpc2_metadata_entry *out_entry);

/**
 * Base64 decodes a binary metadata value (for headers ending with '-bin').
 * Allocates buffer using ezgrpc2_malloc. Caller frees with ezgrpc2_free.
 * Returns pointer to decoded bytes, and sets out_len. Returns nullptr on error.
 */
EZGRPC2_API uint8_t *ezgrpc2_metadata_decode_bin(const char *base64_str, size_t in_len,
                                                 size_t *out_len);

/**
 * Base64 encodes binary data for a '-bin' header.
 * Allocates null-terminated string using ezgrpc2_malloc. Caller frees with ezgrpc2_free.
 * Returns pointer to base64 string, and sets out_len if non-null. Returns nullptr on error.
 */
EZGRPC2_API char *ezgrpc2_metadata_encode_bin(const uint8_t *data, size_t in_len, size_t *out_len);

/**
 * Send custom initial metadata (HTTP/2 response headers) on a stream before data frames.
 */
EZGRPC2_API int ezgrpc2_server_session_stream_send_initial_metadata(
    ezgrpc2_server *server, ezgrpc2_session_uuid session_uuid, int32_t stream_id,
    const ezgrpc2_metadata_entry *entries, size_t count);

/**
 * End a stream with status code, optional custom status message, and custom trailing metadata.
 */
EZGRPC2_API int ezgrpc2_server_session_stream_end2(ezgrpc2_server *server,
                                                   ezgrpc2_session_uuid session_uuid,
                                                   int32_t stream_id, ezgrpc2_grpc_status status,
                                                   const char *custom_status_message,
                                                   const ezgrpc2_metadata_entry *trailers,
                                                   size_t nb_trailers);

#endif /* EZGRPC2_METADATA_H */
