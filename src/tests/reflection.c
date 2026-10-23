#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grpc.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_reflection(void) {
  grpc_server *server = grpc_server_new(
      &(grpc_config){.port = 19994, .ipv4_addr = "127.0.0.1", .log_level = GRPC_LOG_OFF});
  TEST_ASSERT_NOT_NULL(server);

  TEST_ASSERT_EQUAL_INT(GRPC_OK, grpc_reflection_enable(server));
  TEST_ASSERT_EQUAL_INT(GRPC_ERR_EXISTS, grpc_reflection_enable(server));

  const uint8_t descriptor[] = {0x0a, 0x0e, 'h', 'e', 'l', 'l', 'o', 'w', 'o',
                                'r',  'l',  'd', '.', 'p', 'r', 'o', 't', 'o'};
  TEST_ASSERT_EQUAL_INT(GRPC_OK,
                        grpc_reflection_add_file_descriptor(server, "helloworld.proto",
                                                               descriptor, sizeof(descriptor)));
  TEST_ASSERT_EQUAL_INT(GRPC_ERR_INVALID,
                        grpc_reflection_add_file_descriptor(server, nullptr, descriptor, 1));
  TEST_ASSERT_EQUAL_INT(GRPC_ERR_INVALID,
                        grpc_reflection_add_file_descriptor(server, "x", nullptr, 0));

  grpc_server_free(server);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_reflection);
  return UNITY_END();
}
