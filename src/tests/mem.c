#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#define EZGRPC2_NO_ALLOC_OVERRIDE
#include "ezgrpc2_mem.h"
#include "unity.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_allocator_name(void) {
  const char *name = ezgrpc2_get_allocator_name();
  TEST_ASSERT_NOT_NULL(name);
#if defined(EZGRPC2_USE_MIMALLOC)
  TEST_ASSERT_EQUAL_STRING("mimalloc", name);
#elif defined(EZGRPC2_USE_JEMALLOC)
  TEST_ASSERT_EQUAL_STRING("jemalloc", name);
#elif defined(EZGRPC2_USE_CUSTOM_ALLOCATOR)
  TEST_ASSERT_EQUAL_STRING("custom", name);
#else
  TEST_ASSERT_EQUAL_STRING("default (libc)", name);
#endif
}

void test_allocator_basic_operations(void) {
  // Test ezgrpc2_malloc & ezgrpc2_free
  void *p1 = ezgrpc2_malloc(128);
  TEST_ASSERT_NOT_NULL(p1);
  memset(p1, 0xAA, 128);
  ezgrpc2_free(p1);

  // Test ezgrpc2_calloc
  int *p2 = (int *)ezgrpc2_calloc(16, sizeof(int));
  TEST_ASSERT_NOT_NULL(p2);
  for (int i = 0; i < 16; i++) {
    TEST_ASSERT_EQUAL_INT(0, p2[i]);
  }

  // Test ezgrpc2_realloc
  p2 = (int *)ezgrpc2_realloc(p2, 32 * sizeof(int));
  TEST_ASSERT_NOT_NULL(p2);
  for (int i = 0; i < 16; i++) {
    TEST_ASSERT_EQUAL_INT(0, p2[i]);
  }
  ezgrpc2_free(p2);

  // Test ezgrpc2_strdup
  char *str = ezgrpc2_strdup("hello ezgrpc2");
  TEST_ASSERT_NOT_NULL(str);
  TEST_ASSERT_EQUAL_STRING("hello ezgrpc2", str);
  ezgrpc2_free(str);

  // Test ezgrpc2_strndup
  char *nstr = ezgrpc2_strndup("hello ezgrpc2", 5);
  TEST_ASSERT_NOT_NULL(nstr);
  TEST_ASSERT_EQUAL_STRING("hello", nstr);
  ezgrpc2_free(nstr);

  // Test ezgrpc2_aligned_alloc (64 bytes aligned)
  void *aligned = ezgrpc2_aligned_alloc(64, 256);
  TEST_ASSERT_NOT_NULL(aligned);
  TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)aligned % 64);
  ezgrpc2_free(aligned);
}

static size_t g_mock_malloc_count = 0;
static size_t g_mock_free_count = 0;

static void *mock_malloc(size_t size) {
  g_mock_malloc_count++;
  return malloc(size);
}

static void mock_free(void *ptr) {
  g_mock_free_count++;
  free(ptr);
}

void test_custom_allocator_switch(void) {
  const ezgrpc2_allocator *orig = ezgrpc2_get_allocator();
  TEST_ASSERT_NOT_NULL(orig);

  ezgrpc2_allocator mock_alloc = {
    .malloc = mock_malloc,
    .free = mock_free,
  };

  ezgrpc2_set_allocator(&mock_alloc);
  const ezgrpc2_allocator *current = ezgrpc2_get_allocator();
  TEST_ASSERT_NOT_NULL(current);
  TEST_ASSERT_TRUE(current->malloc == mock_malloc);
  TEST_ASSERT_TRUE(current->free == mock_free);

#if defined(EZGRPC2_USE_CUSTOM_ALLOCATOR)
  g_mock_malloc_count = 0;
  g_mock_free_count = 0;
  void *ptr = ezgrpc2_malloc(64);
  TEST_ASSERT_NOT_NULL(ptr);
  TEST_ASSERT_EQUAL_UINT64(1, g_mock_malloc_count);
  ezgrpc2_free(ptr);
  TEST_ASSERT_EQUAL_UINT64(1, g_mock_free_count);
#endif

  // Restore default
  ezgrpc2_set_allocator(nullptr);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_allocator_name);
  RUN_TEST(test_allocator_basic_operations);
  RUN_TEST(test_custom_allocator_switch);
  return UNITY_END();
}
