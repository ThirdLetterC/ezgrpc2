#define EZGRPC2_NO_ALLOC_OVERRIDE
#include "ezgrpc2_mem.h"

#if defined(EZGRPC2_USE_MIMALLOC)
static const char *g_allocator_name = "mimalloc";
#elif defined(EZGRPC2_USE_JEMALLOC)
static const char *g_allocator_name = "jemalloc";
#elif defined(EZGRPC2_USE_CUSTOM_ALLOCATOR)
static const char *g_allocator_name = "custom";
#else
static const char *g_allocator_name = "default (libc)";
#endif

static void *default_malloc(size_t size) {
#if defined(EZGRPC2_USE_MIMALLOC)
  return mi_malloc(size);
#else
  return malloc(size);
#endif
}

static void *default_calloc(size_t count, size_t size) {
#if defined(EZGRPC2_USE_MIMALLOC)
  return mi_calloc(count, size);
#else
  return calloc(count, size);
#endif
}

static void *default_realloc(void *ptr, size_t newsize) {
#if defined(EZGRPC2_USE_MIMALLOC)
  return mi_realloc(ptr, newsize);
#else
  return realloc(ptr, newsize);
#endif
}

static void default_free(void *ptr) {
#if defined(EZGRPC2_USE_MIMALLOC)
  mi_free(ptr);
#else
  free(ptr);
#endif
}

static void *default_aligned_alloc(size_t alignment, size_t size) {
#if defined(EZGRPC2_USE_MIMALLOC)
  return mi_aligned_alloc(alignment, size);
#else
  return aligned_alloc(alignment, size);
#endif
}

static char *default_strndup(const char *s, size_t n) {
#if defined(EZGRPC2_USE_MIMALLOC)
  return mi_strndup(s, n);
#else
  if (s == nullptr)
    return nullptr;
  size_t l = 0;
  while (l < n && s[l] != '\0')
    l++;
  char *r = default_malloc(l + 1);
  if (r != nullptr) {
    memcpy(r, s, l);
    r[l] = '\0';
  }
  return r;
#endif
}

static ezgrpc2_allocator g_ezgrpc2_default_allocator = {
    .malloc = default_malloc,
    .calloc = default_calloc,
    .realloc = default_realloc,
    .free = default_free,
    .aligned_alloc = default_aligned_alloc,
    .strndup = default_strndup,
};

ezgrpc2_allocator g_ezgrpc2_current_allocator = {
    .malloc = default_malloc,
    .calloc = default_calloc,
    .realloc = default_realloc,
    .free = default_free,
    .aligned_alloc = default_aligned_alloc,
    .strndup = default_strndup,
};

EZGRPC2_API void ezgrpc2_set_allocator(const ezgrpc2_allocator *allocator) {
  if (allocator == nullptr) {
    g_ezgrpc2_current_allocator = g_ezgrpc2_default_allocator;
    return;
  }
  if (allocator->malloc)
    g_ezgrpc2_current_allocator.malloc = allocator->malloc;
  if (allocator->calloc)
    g_ezgrpc2_current_allocator.calloc = allocator->calloc;
  if (allocator->realloc)
    g_ezgrpc2_current_allocator.realloc = allocator->realloc;
  if (allocator->free)
    g_ezgrpc2_current_allocator.free = allocator->free;
  if (allocator->aligned_alloc)
    g_ezgrpc2_current_allocator.aligned_alloc = allocator->aligned_alloc;
  if (allocator->strndup)
    g_ezgrpc2_current_allocator.strndup = allocator->strndup;
}

EZGRPC2_API const ezgrpc2_allocator *ezgrpc2_get_allocator(void) {
  return &g_ezgrpc2_current_allocator;
}

EZGRPC2_API const char *ezgrpc2_get_allocator_name(void) {
  return g_allocator_name;
}
