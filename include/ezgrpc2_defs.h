#ifndef EZGRPC2_DEFS_H
#define EZGRPC2_DEFS_H

#include <stdbool.h>
#include <stddef.h>
#include <stdint.h>

#ifndef EZGRPC2_API
#define EZGRPC2_API [[gnu::visibility("default")]]
#endif

/* Provide nullptr for compilers that accept C2x but lack the C23 keyword. */
#if !defined(__STDC_VERSION__) || __STDC_VERSION__ < 202311L
#ifndef nullptr
#define nullptr ((void *)0)
#endif
#endif

#endif /* EZGRPC2_DEFS_H */
