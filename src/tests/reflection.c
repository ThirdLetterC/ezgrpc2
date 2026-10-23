#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ezgrpc2.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_reflection_registration(void) {
  ezgrpc2_server *server = ezgrpc2_server_new("0.0.0.0", 19997, nullptr, 0, 16, nullptr, nullptr);
  TEST_ASSERT_NOT_NULL(server);

  int rc = ezgrpc2_reflection_register(server);
  TEST_ASSERT_EQUAL_INT(0, rc);

  ezgrpc2_server_free(server);
}

void test_reflection_file_descriptor_add(void) {
  const uint8_t sample_desc[] = {0x0a, 0x0e, 0x74, 0x65, 0x73, 0x74,
                                 0x2e, 0x70, 0x72, 0x6f, 0x74, 0x6f};
  int rc = ezgrpc2_reflection_add_file_descriptor("test.proto", sample_desc, sizeof(sample_desc));
  TEST_ASSERT_EQUAL_INT(0, rc);

  // Invalid arguments
  TEST_ASSERT_EQUAL_INT(
      1, ezgrpc2_reflection_add_file_descriptor(nullptr, sample_desc, sizeof(sample_desc)));
  TEST_ASSERT_EQUAL_INT(1, ezgrpc2_reflection_add_file_descriptor("test.proto", nullptr, 0));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_reflection_registration);
  RUN_TEST(test_reflection_file_descriptor_add);
  return UNITY_END();
}
