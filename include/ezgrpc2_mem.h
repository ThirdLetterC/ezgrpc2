#pragma once

#include "ezgrpc2_defs.h"

/**
 * The allocator used for everything ezgrpc2 allocates, including the
 * buffers it hands to the application (to be released with ezgrpc2_free()).
 * It defaults to the one the library was built with (libc, mimalloc or
 * jemalloc).
 */
typedef struct ezgrpc2_allocator {
  void *(*malloc)(size_t size);
  void *(*calloc)(size_t nmemb, size_t size);
  void *(*realloc)(void *ptr, size_t size);
  void (*free)(void *ptr);
  void *(*aligned_alloc)(size_t alignment, size_t size);
} ezgrpc2_allocator;

/**
 * Replaces the allocator; nullptr restores the default. Call it before
 * creating any server: memory must be freed by the allocator that allocated it.
 * Functions left nullptr keep their current value.
 */
EZGRPC2_API void ezgrpc2_set_allocator(const ezgrpc2_allocator *allocator);

/** The allocator in use. */
EZGRPC2_API const ezgrpc2_allocator *ezgrpc2_get_allocator(void);

/** The name of the default allocator: "libc", "mimalloc" or "jemalloc". */
EZGRPC2_API const char *ezgrpc2_get_allocator_name(void);

EZGRPC2_API void *ezgrpc2_malloc(size_t size);
EZGRPC2_API void *ezgrpc2_calloc(size_t nmemb, size_t size);
EZGRPC2_API void *ezgrpc2_realloc(void *ptr, size_t size);
EZGRPC2_API void ezgrpc2_free(void *ptr);
EZGRPC2_API void *ezgrpc2_aligned_alloc(size_t alignment, size_t size);
EZGRPC2_API char *ezgrpc2_strdup(const char *s);
EZGRPC2_API char *ezgrpc2_strndup(const char *s, size_t n);
