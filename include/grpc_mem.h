#pragma once

#include "grpc_defs.h"

/**
 * The allocator used for everything grpc allocates, including the
 * buffers it hands to the application (to be released with grpc_free()).
 * It defaults to the one the library was built with (libc, mimalloc or
 * jemalloc).
 */
typedef struct grpc_allocator {
  void *(*malloc)(size_t size);
  void *(*calloc)(size_t nmemb, size_t size);
  void *(*realloc)(void *ptr, size_t size);
  void (*free)(void *ptr);
  void *(*aligned_alloc)(size_t alignment, size_t size);
} grpc_allocator;

/**
 * Replaces the allocator; nullptr restores the default. Call it before
 * creating any server: memory must be freed by the allocator that allocated it.
 * Functions left nullptr keep their current value.
 */
GRPC_API void grpc_set_allocator(const grpc_allocator *allocator);

/** The allocator in use. */
GRPC_API const grpc_allocator *grpc_get_allocator(void);

/** The name of the default allocator: "libc", "mimalloc" or "jemalloc". */
GRPC_API const char *grpc_get_allocator_name(void);

GRPC_API void *grpc_malloc(size_t size);
GRPC_API void *grpc_calloc(size_t nmemb, size_t size);
GRPC_API void *grpc_realloc(void *ptr, size_t size);
GRPC_API void grpc_free(void *ptr);
GRPC_API void *grpc_aligned_alloc(size_t alignment, size_t size);
GRPC_API char *grpc_strdup(const char *s);
GRPC_API char *grpc_strndup(const char *s, size_t n);
