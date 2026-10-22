#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "ezgrpc2_protobuf_c.h"
#include "test_msg.pb-c.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_pb_allocator(void) {
  ProtobufCAllocator *allocator = ezgrpc2_pb_get_allocator();
  TEST_ASSERT_NOT_NULL(allocator);
  TEST_ASSERT_NOT_NULL(allocator->alloc);
  TEST_ASSERT_NOT_NULL(allocator->free);

  // Allocate and free memory through allocator
  void *ptr = allocator->alloc(allocator->allocator_data, 128);
  TEST_ASSERT_NOT_NULL(ptr);
  memset(ptr, 0x42, 128);
  allocator->free(allocator->allocator_data, ptr);
}

void test_pb_pack_and_unpack(void) {
  Test__Echo echo = TEST__ECHO__INIT;
  echo.text = (char *)"Hello from ezgrpc2 protobuf-c integration!";
  echo.number = 42;

  // Pack into ezgrpc2_message
  ezgrpc2_message *msg = ezgrpc2_pb_pack((ProtobufCMessage *)&echo, 0);
  TEST_ASSERT_NOT_NULL(msg);
  TEST_ASSERT_EQUAL_UINT8(0, msg->cflag);
  TEST_ASSERT_GREATER_THAN_UINT32(0, msg->len);

  // Unpack from ezgrpc2_message
  Test__Echo *unpacked = EZGRPC2_PB_UNPACK(Test__Echo, &test__echo__descriptor, msg);
  TEST_ASSERT_NOT_NULL(unpacked);
  TEST_ASSERT_EQUAL_STRING("Hello from ezgrpc2 protobuf-c integration!", unpacked->text);
  TEST_ASSERT_EQUAL_INT32(42, unpacked->number);

  // Clean up
  EZGRPC2_PB_FREE_UNPACKED(unpacked);
  ezgrpc2_message_free(msg);
}

void test_pb_create_response(void) {
  Test__Echo echo = TEST__ECHO__INIT;
  echo.text = (char *)"Response Test";
  echo.number = 100;

  ezgrpc2_messages *msgs = ezgrpc2_pb_create_response((ProtobufCMessage *)&echo);
  TEST_ASSERT_NOT_NULL(msgs);
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)ezgrpc2_messages_count(msgs));

  ezgrpc2_message *msg = ezgrpc2_messages_pop(msgs);
  TEST_ASSERT_NOT_NULL(msg);

  Test__Echo *unpacked = EZGRPC2_PB_UNPACK(Test__Echo, &test__echo__descriptor, msg);
  TEST_ASSERT_NOT_NULL(unpacked);
  TEST_ASSERT_EQUAL_STRING("Response Test", unpacked->text);
  TEST_ASSERT_EQUAL_INT32(100, unpacked->number);

  EZGRPC2_PB_FREE_UNPACKED(unpacked);
  ezgrpc2_message_free(msg);
  ezgrpc2_messages_free(msgs);
}

void test_pb_null_safety(void) {
  TEST_ASSERT_NULL(ezgrpc2_pb_pack(nullptr, 0));
  TEST_ASSERT_NULL(ezgrpc2_pb_unpack(nullptr, nullptr));
  TEST_ASSERT_NULL(ezgrpc2_pb_create_response(nullptr));
  // Freeing null should not crash
  ezgrpc2_pb_free_unpacked(nullptr);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pb_allocator);
  RUN_TEST(test_pb_pack_and_unpack);
  RUN_TEST(test_pb_create_response);
  RUN_TEST(test_pb_null_safety);
  return UNITY_END();
}
