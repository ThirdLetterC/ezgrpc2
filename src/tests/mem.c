#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define GRPC_NO_ALLOC_OVERRIDE
#include "grpc_mem.h"
#include "unity.h"

void setUp(void) {}

void tearDown(void) {}

void test_allocator_name(void) {
  const char *name = grpc_get_allocator_name();
  TEST_ASSERT_NOT_NULL(name);
#if defined(GRPC_USE_MIMALLOC)
  TEST_ASSERT_EQUAL_STRING("mimalloc", name);
#elif defined(GRPC_USE_JEMALLOC)
  TEST_ASSERT_EQUAL_STRING("jemalloc", name);
#else
  TEST_ASSERT_EQUAL_STRING("libc", name);
#endif
}

void test_allocator_basic_operations(void) {
  // Test grpc_malloc & grpc_free
  void *p1 = grpc_malloc(128);
  TEST_ASSERT_NOT_NULL(p1);
  memset(p1, 0xAA, 128);
  grpc_free(p1);

  // Test grpc_calloc
  int *p2 = (int *)grpc_calloc(16, sizeof(int));
  TEST_ASSERT_NOT_NULL(p2);
  for (int i = 0; i < 16; i++) {
    TEST_ASSERT_EQUAL_INT(0, p2[i]);
  }

  // Test grpc_realloc
  p2 = (int *)grpc_realloc(p2, 32 * sizeof(int));
  TEST_ASSERT_NOT_NULL(p2);
  for (int i = 0; i < 16; i++) {
    TEST_ASSERT_EQUAL_INT(0, p2[i]);
  }
  grpc_free(p2);

  // Test grpc_strdup
  char *str = grpc_strdup("hello grpc");
  TEST_ASSERT_NOT_NULL(str);
  TEST_ASSERT_EQUAL_STRING("hello grpc", str);
  grpc_free(str);

  // Test grpc_strndup
  char *nstr = grpc_strndup("hello grpc", 5);
  TEST_ASSERT_NOT_NULL(nstr);
  TEST_ASSERT_EQUAL_STRING("hello", nstr);
  grpc_free(nstr);

  // Test grpc_aligned_alloc (64 bytes aligned)
  void *aligned = grpc_aligned_alloc(64, 256);
  TEST_ASSERT_NOT_NULL(aligned);
  TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)aligned % 64);
  grpc_free(aligned);
}

#if defined(GRPC_USE_CUSTOM_ALLOCATOR)
static size_t g_mock_malloc_count = 0;
static size_t g_mock_free_count = 0;
#endif

static void *mock_malloc(size_t size) {
#if defined(GRPC_USE_CUSTOM_ALLOCATOR)
  g_mock_malloc_count++;
#endif
  return malloc(size);
}

static void mock_free(void *ptr) {
#if defined(GRPC_USE_CUSTOM_ALLOCATOR)
  g_mock_free_count++;
#endif
  free(ptr);
}

void test_custom_allocator_switch(void) {
  const grpc_allocator *orig = grpc_get_allocator();
  TEST_ASSERT_NOT_NULL(orig);

  grpc_allocator mock_alloc = {
      .malloc = mock_malloc,
      .free = mock_free,
  };

  grpc_set_allocator(&mock_alloc);
  const grpc_allocator *current = grpc_get_allocator();
  TEST_ASSERT_NOT_NULL(current);
  TEST_ASSERT_TRUE(current->malloc == mock_malloc);
  TEST_ASSERT_TRUE(current->free == mock_free);

#if defined(GRPC_USE_CUSTOM_ALLOCATOR)
  g_mock_malloc_count = 0;
  g_mock_free_count = 0;
  void *ptr = grpc_malloc(64);
  TEST_ASSERT_NOT_NULL(ptr);
  TEST_ASSERT_EQUAL_UINT64(1, g_mock_malloc_count);
  grpc_free(ptr);
  TEST_ASSERT_EQUAL_UINT64(1, g_mock_free_count);
#endif

  // Restore default
  grpc_set_allocator(nullptr);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_allocator_name);
  RUN_TEST(test_allocator_basic_operations);
  RUN_TEST(test_custom_allocator_switch);
  return UNITY_END();
}
