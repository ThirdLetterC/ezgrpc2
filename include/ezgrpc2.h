/* EZgRPC2 is a gRPC server library in C. */
#pragma once

#include "ezgrpc2_defs.h"
#include "ezgrpc2_error.h"
#include "ezgrpc2_status.h"
#include "ezgrpc2_mem.h"
#include "ezgrpc2_message.h"
#include "ezgrpc2_metadata.h"
#include "ezgrpc2_call.h"
#include "ezgrpc2_event.h"
#include "ezgrpc2_server.h"
#include "ezgrpc2_interceptor.h"
#include "ezgrpc2_status_details.h"
#include "ezgrpc2_health.h"
#include "ezgrpc2_reflection.h"
#include "ezgrpc2_compression.h"
#include "ezgrpc2_tls.h"

#if defined(EZGRPC2_ENABLE_PROTOBUF_C)
#include "ezgrpc2_protobuf_c.h"
#endif

#if defined(EZGRPC2_ENABLE_FLATCC)
#include "ezgrpc2_flatcc.h"
#endif
