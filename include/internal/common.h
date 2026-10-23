#ifndef EZGRPC2_COMMON_H
#define EZGRPC2_COMMON_H

/* private to the library: never included by a public header */

#include <stdint.h>
#include <stdlib.h>
#include <string.h>
#include "ezgrpc2_defs.h"

typedef char i8;
typedef uint8_t u8;
typedef int8_t s8;
typedef int16_t i16;
typedef uint16_t u16;
typedef int32_t i32;
typedef uint32_t u32;
typedef int64_t i64;
typedef uint64_t u64;

#ifndef alignof
#if (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#define alignof _Alignof
#endif
#endif

#define REALIGN(alignment, size) (((size) + ((alignment) - 1)) & ~((alignment) - 1))

#if __has_builtin(__builtin_expect)
#define likely(x)   __builtin_expect(!!(x), 1)
#define unlikely(x) __builtin_expect(!!(x), 0)
#else
#define likely(x)   (x)
#define unlikely(x) (x)
#endif

#ifndef CONFIG_STREAM_MAX_HEADERS
#define CONFIG_STREAM_MAX_HEADERS 32
#endif

#endif
