#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ezgrpc2.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static ezgrpc2_server *new_server(uint16_t port) {
  return ezgrpc2_server_new(
      &(ezgrpc2_config){.port = port, .ipv4_addr = "127.0.0.1", .log_level = EZGRPC2_LOG_OFF});
}

void test_health_status_get_set(void) {
  ezgrpc2_server *server = new_server(19999);
  TEST_ASSERT_NOT_NULL(server);
  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_health_enable(server));

  /* everything is SERVING until told otherwise */
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_SERVING, ezgrpc2_health_get_status(server, nullptr));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_SERVING, ezgrpc2_health_get_status(server, ""));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_SERVING, ezgrpc2_health_get_status(server, "any.Service"));

  ezgrpc2_health_set_status(server, nullptr, EZGRPC2_HEALTH_NOT_SERVING);
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_NOT_SERVING, ezgrpc2_health_get_status(server, nullptr));

  ezgrpc2_health_set_status(server, "helloworld.Greeter", EZGRPC2_HEALTH_SERVING);
  ezgrpc2_health_set_status(server, "order.OrderService", EZGRPC2_HEALTH_NOT_SERVING);
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_SERVING,
                        ezgrpc2_health_get_status(server, "helloworld.Greeter"));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_NOT_SERVING,
                        ezgrpc2_health_get_status(server, "order.OrderService"));
  ezgrpc2_health_set_status(server, "order.OrderService", EZGRPC2_HEALTH_SERVING);
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_SERVING,
                        ezgrpc2_health_get_status(server, "order.OrderService"));

  ezgrpc2_server_free(server);
}

void test_health_is_per_server(void) {
  ezgrpc2_server *a = new_server(19998);
  ezgrpc2_server *b = new_server(19997);
  TEST_ASSERT_NOT_NULL(a);
  TEST_ASSERT_NOT_NULL(b);
  ezgrpc2_health_set_status(a, nullptr, EZGRPC2_HEALTH_NOT_SERVING);
  ezgrpc2_health_set_status(b, nullptr, EZGRPC2_HEALTH_SERVING);
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_NOT_SERVING, ezgrpc2_health_get_status(a, nullptr));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_HEALTH_SERVING, ezgrpc2_health_get_status(b, nullptr));
  ezgrpc2_server_free(a);
  ezgrpc2_server_free(b);
}

void test_health_enable_twice(void) {
  ezgrpc2_server *server = new_server(19996);
  TEST_ASSERT_NOT_NULL(server);
  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_health_enable(server));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_EXISTS, ezgrpc2_health_enable(server));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID, ezgrpc2_health_enable(nullptr));
  ezgrpc2_server_free(server);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_health_status_get_set);
  RUN_TEST(test_health_is_per_server);
  RUN_TEST(test_health_enable_twice);
  return UNITY_END();
}
