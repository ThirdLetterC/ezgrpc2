#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "grpc_protobuf_c.h"
#include "test_msg.pb-c.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_pb_allocator(void) {
  ProtobufCAllocator *allocator = grpc_pb_get_allocator();
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
  echo.text = (char *)"Hello from grpc protobuf-c integration!";
  echo.number = 42;

  // Pack into grpc_message
  grpc_message *msg = grpc_pb_pack((ProtobufCMessage *)&echo);
  TEST_ASSERT_NOT_NULL(msg);
  TEST_ASSERT_EQUAL_UINT8(0, msg->cflag);
  TEST_ASSERT_GREATER_THAN_UINT32(0, msg->len);

  // Unpack from grpc_message
  Test__Echo *unpacked = GRPC_PB_UNPACK(Test__Echo, &test__echo__descriptor, msg);
  TEST_ASSERT_NOT_NULL(unpacked);
  TEST_ASSERT_EQUAL_STRING("Hello from grpc protobuf-c integration!", unpacked->text);
  TEST_ASSERT_EQUAL_INT32(42, unpacked->number);

  // Clean up
  GRPC_PB_FREE_UNPACKED(unpacked);
  grpc_message_free(msg);
}

void test_pb_send_null_safety(void) {
  Test__Echo echo = TEST__ECHO__INIT;
  TEST_ASSERT_EQUAL_INT(GRPC_ERR_INVALID, grpc_call_send_pb(nullptr, &echo.base));
  TEST_ASSERT_EQUAL_INT(GRPC_ERR_INVALID, grpc_call_send_pb(nullptr, nullptr));
}

void test_pb_null_safety(void) {
  TEST_ASSERT_NULL(grpc_pb_pack(nullptr));
  TEST_ASSERT_NULL(grpc_pb_unpack(nullptr, nullptr));
  // Freeing null should not crash
  grpc_pb_free_unpacked(nullptr);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_pb_allocator);
  RUN_TEST(test_pb_pack_and_unpack);
  RUN_TEST(test_pb_send_null_safety);
  RUN_TEST(test_pb_null_safety);
  return UNITY_END();
}
