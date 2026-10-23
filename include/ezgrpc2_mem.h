#ifndef EZGRPC2_MEM_H
#define EZGRPC2_MEM_H

#include <stdlib.h>
#include <string.h>
#include "defs.h"

#ifdef __cplusplus
extern "C" {
#endif

typedef struct ezgrpc2_allocator {
  void *(*malloc)(size_t size);
  void *(*calloc)(size_t nmemb, size_t size);
  void *(*realloc)(void *ptr, size_t newsize);
  void (*free)(void *ptr);
  void *(*aligned_alloc)(size_t alignment, size_t size);
  char *(*strndup)(const char *s, size_t n);
} ezgrpc2_allocator;

/* Set/get custom allocator at runtime */
EZGRPC2_API void ezgrpc2_set_allocator(const ezgrpc2_allocator *allocator);
EZGRPC2_API const ezgrpc2_allocator *ezgrpc2_get_allocator(void);
EZGRPC2_API const char *ezgrpc2_get_allocator_name(void);

#if defined(EZGRPC2_USE_MIMALLOC)
#include <mimalloc.h>

static inline void *ezgrpc2_malloc(size_t size) {
  return mi_malloc(size);
}
static inline void *ezgrpc2_calloc(size_t nmemb, size_t size) {
  return mi_calloc(nmemb, size);
}
static inline void *ezgrpc2_realloc(void *ptr, size_t newsize) {
  return mi_realloc(ptr, newsize);
}
static inline void ezgrpc2_free(void *ptr) {
  mi_free(ptr);
}
static inline void *ezgrpc2_aligned_alloc(size_t alignment, size_t size) {
  return mi_aligned_alloc(alignment, size);
}
static inline char *ezgrpc2_strdup(const char *s) {
  return mi_strdup(s);
}
static inline char *ezgrpc2_strndup(const char *s, size_t n) {
  return mi_strndup(s, n);
}

#elif defined(EZGRPC2_USE_JEMALLOC)
#include <jemalloc/jemalloc.h>

static inline void *ezgrpc2_malloc(size_t size) {
  return malloc(size);
}
static inline void *ezgrpc2_calloc(size_t nmemb, size_t size) {
  return calloc(nmemb, size);
}
static inline void *ezgrpc2_realloc(void *ptr, size_t newsize) {
  return realloc(ptr, newsize);
}
static inline void ezgrpc2_free(void *ptr) {
  free(ptr);
}
static inline void *ezgrpc2_aligned_alloc(size_t alignment, size_t size) {
  return aligned_alloc(alignment, size);
}
static inline char *ezgrpc2_strdup(const char *s) {
  if (s == nullptr)
    return nullptr;
  size_t len = strlen(s);
  char *r = (char *)malloc(len + 1);
  if (r != nullptr) {
    memcpy(r, s, len + 1);
  }
  return r;
}
static inline char *ezgrpc2_strndup(const char *s, size_t n) {
  if (s == nullptr)
    return nullptr;
  size_t l = 0;
  while (l < n && s[l] != '\0')
    l++;
  char *r = (char *)malloc(l + 1);
  if (r != nullptr) {
    memcpy(r, s, l);
    r[l] = '\0';
  }
  return r;
}

#elif defined(EZGRPC2_USE_CUSTOM_ALLOCATOR)

extern ezgrpc2_allocator g_ezgrpc2_current_allocator;

static inline void *ezgrpc2_malloc(size_t size) {
  return g_ezgrpc2_current_allocator.malloc(size);
}
static inline void *ezgrpc2_calloc(size_t nmemb, size_t size) {
  return g_ezgrpc2_current_allocator.calloc(nmemb, size);
}
static inline void *ezgrpc2_realloc(void *ptr, size_t newsize) {
  return g_ezgrpc2_current_allocator.realloc(ptr, newsize);
}
static inline void ezgrpc2_free(void *ptr) {
  g_ezgrpc2_current_allocator.free(ptr);
}
static inline void *ezgrpc2_aligned_alloc(size_t alignment, size_t size) {
  return g_ezgrpc2_current_allocator.aligned_alloc(alignment, size);
}
static inline char *ezgrpc2_strdup(const char *s) {
  if (s == nullptr)
    return nullptr;
  size_t len = strlen(s);
  return g_ezgrpc2_current_allocator.strndup(s, len);
}
static inline char *ezgrpc2_strndup(const char *s, size_t n) {
  return g_ezgrpc2_current_allocator.strndup(s, n);
}

#else /* Default libc allocator */

static inline void *ezgrpc2_malloc(size_t size) {
  return malloc(size);
}
static inline void *ezgrpc2_calloc(size_t nmemb, size_t size) {
  return calloc(nmemb, size);
}
static inline void *ezgrpc2_realloc(void *ptr, size_t newsize) {
  return realloc(ptr, newsize);
}
static inline void ezgrpc2_free(void *ptr) {
  free(ptr);
}
static inline void *ezgrpc2_aligned_alloc(size_t alignment, size_t size) {
  return aligned_alloc(alignment, size);
}
static inline char *ezgrpc2_strdup(const char *s) {
  if (s == nullptr)
    return nullptr;
  size_t len = strlen(s);
  char *r = (char *)malloc(len + 1);
  if (r != nullptr) {
    memcpy(r, s, len + 1);
  }
  return r;
}
static inline char *ezgrpc2_strndup(const char *s, size_t n) {
  if (s == nullptr)
    return nullptr;
  size_t l = 0;
  while (l < n && s[l] != '\0')
    l++;
  char *r = (char *)malloc(l + 1);
  if (r != nullptr) {
    memcpy(r, s, l);
    r[l] = '\0';
  }
  return r;
}

#endif

#ifndef EZGRPC2_NO_ALLOC_OVERRIDE
#define malloc(sz)            ezgrpc2_malloc(sz)
#define calloc(n, sz)         ezgrpc2_calloc(n, sz)
#define realloc(ptr, sz)      ezgrpc2_realloc(ptr, sz)
#define free(ptr)             ezgrpc2_free(ptr)
#define aligned_alloc(al, sz) ezgrpc2_aligned_alloc(al, sz)
#define strdup(s)             ezgrpc2_strdup(s)
#define strndup(s, n)         ezgrpc2_strndup(s, n)
#endif

#ifdef __cplusplus
}
#endif

#endif /* EZGRPC2_MEM_H */
