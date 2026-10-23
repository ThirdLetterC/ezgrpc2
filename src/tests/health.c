#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ezgrpc2.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_health_status_get_set(void) {
  // Initial default overall status
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_SERVING, ezgrpc2_health_get_status(nullptr));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_SERVING, ezgrpc2_health_get_status(""));

  // Change overall status
  ezgrpc2_health_set_status(nullptr, EZGRPC2_HEALTH_NOT_SERVING);
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_NOT_SERVING, ezgrpc2_health_get_status(nullptr));

  // Set specific service status
  ezgrpc2_health_set_status("helloworld.Greeter", EZGRPC2_HEALTH_SERVING);
  ezgrpc2_health_set_status("order.OrderService", EZGRPC2_HEALTH_NOT_SERVING);

  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_SERVING, ezgrpc2_health_get_status("helloworld.Greeter"));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_NOT_SERVING,
                        ezgrpc2_health_get_status("order.OrderService"));

  // Reset overall status
  ezgrpc2_health_set_status("", EZGRPC2_HEALTH_SERVING);
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_SERVING, ezgrpc2_health_get_status(""));
}

void test_health_register(void) {
  ezgrpc2_server *server = ezgrpc2_server_new("0.0.0.0", 19999, nullptr, 0, 16, nullptr, nullptr);
  TEST_ASSERT_NOT_NULL(server);

  int rc = ezgrpc2_health_register(server);
  TEST_ASSERT_EQUAL_INT(0, rc);

  ezgrpc2_server_free(server);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_health_status_get_set);
  RUN_TEST(test_health_register);
  return UNITY_END();
}
