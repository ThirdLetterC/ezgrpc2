#ifndef EZGRPC2_DEFS_H
#define EZGRPC2_DEFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef EZGRPC2_API
#define EZGRPC2_API __attribute__((visibility("default")))
#endif

/* the headers are written in C23; this keeps them usable from C11 and C17 */
#if !defined(nullptr) && (!defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L)
#define nullptr ((void *)0)
#endif

#endif /* EZGRPC2_DEFS_H */
