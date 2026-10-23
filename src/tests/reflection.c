#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ezgrpc2.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_reflection(void) {
  ezgrpc2_server *server = ezgrpc2_server_new(
      &(ezgrpc2_config){.port = 19994, .ipv4_addr = "127.0.0.1", .log_level = EZGRPC2_LOG_OFF});
  TEST_ASSERT_NOT_NULL(server);

  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_reflection_enable(server));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_EXISTS, ezgrpc2_reflection_enable(server));

  const uint8_t descriptor[] = {0x0a, 0x0e, 'h', 'e', 'l', 'l', 'o', 'w', 'o',
                                'r',  'l',  'd', '.', 'p', 'r', 'o', 't', 'o'};
  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK,
                        ezgrpc2_reflection_add_file_descriptor(server, "helloworld.proto",
                                                               descriptor, sizeof(descriptor)));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID,
                        ezgrpc2_reflection_add_file_descriptor(server, nullptr, descriptor, 1));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID,
                        ezgrpc2_reflection_add_file_descriptor(server, "x", nullptr, 0));

  ezgrpc2_server_free(server);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_reflection);
  return UNITY_END();
}
