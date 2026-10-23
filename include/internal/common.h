#ifndef EZGRPC2_COMMON_H
#define EZGRPC2_COMMON_H

/* Private library helpers; public headers must not include this file. */

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

/* Round size up to the next multiple of a power-of-two alignment. */
static inline size_t ezgrpc2_align_up(size_t size, size_t alignment) {
  return (size + alignment - 1) & ~(alignment - 1);
}

#ifndef CONFIG_STREAM_MAX_HEADERS
#define CONFIG_STREAM_MAX_HEADERS 32
#endif

#endif
