#include <stdlib.h>
#include <string.h>
#include "ezgrpc2_mem.h"

#if defined(EZGRPC2_USE_MIMALLOC)
#include <mimalloc.h>
static constexpr char default_allocator_name[] = "mimalloc";
static const ezgrpc2_allocator default_allocator = {mi_malloc, mi_calloc, mi_realloc, mi_free,
                                                    mi_aligned_alloc};
static ezgrpc2_allocator current = {mi_malloc, mi_calloc, mi_realloc, mi_free,
                                    mi_aligned_alloc};
#elif defined(EZGRPC2_USE_JEMALLOC)
/* jemalloc replaces the libc functions when linked */
static constexpr char default_allocator_name[] = "jemalloc";
static const ezgrpc2_allocator default_allocator = {malloc, calloc, realloc, free, aligned_alloc};
static ezgrpc2_allocator current = {malloc, calloc, realloc, free, aligned_alloc};
#else
static constexpr char default_allocator_name[] = "libc";
static const ezgrpc2_allocator default_allocator = {malloc, calloc, realloc, free, aligned_alloc};
static ezgrpc2_allocator current = {malloc, calloc, realloc, free, aligned_alloc};
#endif

EZGRPC2_API void ezgrpc2_set_allocator(const ezgrpc2_allocator *allocator) {
  if (allocator == nullptr) {
    current = default_allocator;
    return;
  }
  if (allocator->malloc)
    current.malloc = allocator->malloc;
  if (allocator->calloc)
    current.calloc = allocator->calloc;
  if (allocator->realloc)
    current.realloc = allocator->realloc;
  if (allocator->free)
    current.free = allocator->free;
  if (allocator->aligned_alloc)
    current.aligned_alloc = allocator->aligned_alloc;
}

EZGRPC2_API const ezgrpc2_allocator *ezgrpc2_get_allocator(void) {
  return &current;
}

EZGRPC2_API const char *ezgrpc2_get_allocator_name(void) {
  return default_allocator_name;
}

EZGRPC2_API void *ezgrpc2_malloc(size_t size) {
  return current.malloc(size);
}

EZGRPC2_API void *ezgrpc2_calloc(size_t nmemb, size_t size) {
  return current.calloc(nmemb, size);
}

EZGRPC2_API void *ezgrpc2_realloc(void *ptr, size_t size) {
  return current.realloc(ptr, size);
}

EZGRPC2_API void ezgrpc2_free(void *ptr) {
  current.free(ptr);
}

EZGRPC2_API void *ezgrpc2_aligned_alloc(size_t alignment, size_t size) {
  return current.aligned_alloc(alignment, size);
}

EZGRPC2_API char *ezgrpc2_strndup(const char *s, size_t n) {
  if (s == nullptr)
    return nullptr;
  size_t len = strnlen(s, n);
  char *r = current.malloc(len + 1);
  if (r != nullptr) {
    memcpy(r, s, len);
    r[len] = '\0';
  }
  return r;
}

EZGRPC2_API char *ezgrpc2_strdup(const char *s) {
  return s != nullptr ? ezgrpc2_strndup(s, strlen(s)) : nullptr;
}
