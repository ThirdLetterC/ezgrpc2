#pragma once
#include <sys/types.h>
#include <nghttp2/nghttp2.h>
#include "common.h"
#include "ezgrpc2_defs.h"

void server_setup_ngcallbacks(nghttp2_session_callbacks *ngcallbacks);
ssize_t data_source_read_callback2(nghttp2_session *session, i32 stream_id, u8 *buf, size_t buf_len,
                                   u32 *data_flags, nghttp2_data_source *source, void *user_data);
