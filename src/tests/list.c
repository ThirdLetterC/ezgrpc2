#include <stdio.h>
#include <stdlib.h>
#include <assert.h>
#include "ezgrpc2_list.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_list_creation(void) {
  ezgrpc2_list *list = ezgrpc2_list_new();
  TEST_ASSERT_NOT_NULL(list);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));
  ezgrpc2_list_free(list);
}

void test_list_push_and_pop_back_1(void) {
  ezgrpc2_list *list = ezgrpc2_list_new();
  TEST_ASSERT_NOT_NULL(list);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));
  ezgrpc2_list_push_back(list, (void *)"Alpha");
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)ezgrpc2_list_count(list));
  TEST_ASSERT_EQUAL_MEMORY("Alpha", ezgrpc2_list_pop_back(list), 6);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));
  ezgrpc2_list_free(list);
}

void test_list_push_and_pop_back_2(void) {
  ezgrpc2_list *list = ezgrpc2_list_new();
  TEST_ASSERT_NOT_NULL(list);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));
  ezgrpc2_list_push_back(list, (void *)"Alpha");
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)ezgrpc2_list_count(list));
  ezgrpc2_list_push_back(list, (void *)"Beta");
  TEST_ASSERT_EQUAL_UINT64(2, (uint64_t)ezgrpc2_list_count(list));
  TEST_ASSERT_EQUAL_MEMORY("Beta", ezgrpc2_list_pop_back(list), 5);
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)ezgrpc2_list_count(list));
  TEST_ASSERT_EQUAL_MEMORY("Alpha", ezgrpc2_list_pop_back(list), 6);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));
  ezgrpc2_list_free(list);
}

void test_list_push_and_pop_front_1(void) {
  ezgrpc2_list *list = ezgrpc2_list_new();
  TEST_ASSERT_NOT_NULL(list);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));
  ezgrpc2_list_push_front(list, (void *)"Alpha");
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)ezgrpc2_list_count(list));
  TEST_ASSERT_EQUAL_MEMORY("Alpha", ezgrpc2_list_pop_front(list), 6);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));
  ezgrpc2_list_free(list);
}

void test_list_push_and_pop_front_2(void) {
  ezgrpc2_list *list = ezgrpc2_list_new();
  TEST_ASSERT_NOT_NULL(list);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));
  ezgrpc2_list_push_front(list, (void *)"Alpha");
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)ezgrpc2_list_count(list));
  ezgrpc2_list_push_front(list, (void *)"Beta");
  TEST_ASSERT_EQUAL_UINT64(2, (uint64_t)ezgrpc2_list_count(list));
  TEST_ASSERT_EQUAL_MEMORY("Beta", ezgrpc2_list_pop_front(list), 5);
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)ezgrpc2_list_count(list));
  TEST_ASSERT_EQUAL_MEMORY("Alpha", ezgrpc2_list_pop_front(list), 6);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));
  ezgrpc2_list_free(list);
}

void test_list_concat(void) {
  ezgrpc2_list *dst = ezgrpc2_list_new();
  ezgrpc2_list *src = ezgrpc2_list_new();
  ezgrpc2_list_push_back(dst, (void *)"A");
  ezgrpc2_list_push_back(src, (void *)"B");
  ezgrpc2_list_concat_and_empty_src(dst, src);
  TEST_ASSERT_EQUAL_UINT64(2, (uint64_t)ezgrpc2_list_count(dst));
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(src));
  TEST_ASSERT_EQUAL_MEMORY("A", ezgrpc2_list_pop_front(dst), 2);
  TEST_ASSERT_EQUAL_MEMORY("B", ezgrpc2_list_pop_front(dst), 2);
  ezgrpc2_list_free(dst);
  ezgrpc2_list_free(src);
}

void test_list_random(void) {
  ezgrpc2_list *list = ezgrpc2_list_new();
  TEST_ASSERT_NOT_NULL(list);
  TEST_ASSERT_NULL(ezgrpc2_list_pop_back(list));
  TEST_ASSERT_NULL(ezgrpc2_list_pop_front(list));

  ezgrpc2_list_push_front(list, (void *)"Alpha");
  TEST_ASSERT_EQUAL_MEMORY("Alpha", ezgrpc2_list_pop_back(list), 6);
  TEST_ASSERT_NULL(ezgrpc2_list_pop_back(list));
  TEST_ASSERT_NULL(ezgrpc2_list_pop_front(list));
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));

  ezgrpc2_list_push_back(list, (void *)"Beta");
  ezgrpc2_list_push_front(list, (void *)"Gamma");
  TEST_ASSERT_EQUAL_MEMORY("Beta", ezgrpc2_list_pop_back(list), 5);
  TEST_ASSERT_EQUAL_MEMORY("Gamma", ezgrpc2_list_pop_back(list), 6);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));
  TEST_ASSERT_NULL(ezgrpc2_list_pop_back(list));
  TEST_ASSERT_NULL(ezgrpc2_list_pop_front(list));
  ezgrpc2_list_push_back(list, (void *)"Alpha");
  ezgrpc2_list_push_back(list, (void *)"Beta");
  ezgrpc2_list_push_back(list, (void *)"Gamma");

  TEST_ASSERT_EQUAL_UINT64(3, (uint64_t)ezgrpc2_list_count(list));
  TEST_ASSERT_EQUAL_MEMORY("Alpha", ezgrpc2_list_pop_front(list), 6);
  TEST_ASSERT_EQUAL_MEMORY("Beta", ezgrpc2_list_pop_front(list), 5);
  TEST_ASSERT_EQUAL_MEMORY("Gamma", ezgrpc2_list_pop_front(list), 6);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_count(list));
  TEST_ASSERT_NULL(ezgrpc2_list_pop_back(list));
  TEST_ASSERT_NULL(ezgrpc2_list_pop_front(list));

  ezgrpc2_list_free(list);
}

void foreach (const void *data, [[maybe_unused]] const void *userdata) {
  puts((const char *)data);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_list_creation);
  RUN_TEST(test_list_push_and_pop_back_1);
  RUN_TEST(test_list_push_and_pop_back_2);
  RUN_TEST(test_list_push_and_pop_front_1);
  RUN_TEST(test_list_push_and_pop_front_2);
  RUN_TEST(test_list_concat);
  RUN_TEST(test_list_random);
  return UNITY_END();
}
