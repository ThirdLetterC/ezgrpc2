#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grpc.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static grpc_server *new_server(uint16_t port) {
  return grpc_server_new(
      &(grpc_config){.port = port, .ipv4_addr = "127.0.0.1", .log_level = GRPC_LOG_OFF});
}

void test_health_status_get_set(void) {
  grpc_server *server = new_server(19999);
  TEST_ASSERT_NOT_NULL(server);
  TEST_ASSERT_EQUAL_INT(GRPC_OK, grpc_health_enable(server));

  /* everything is SERVING until told otherwise */
  TEST_ASSERT_EQUAL_INT(GRPC_HEALTH_SERVING, grpc_health_get_status(server, nullptr));
  TEST_ASSERT_EQUAL_INT(GRPC_HEALTH_SERVING, grpc_health_get_status(server, ""));
  TEST_ASSERT_EQUAL_INT(GRPC_HEALTH_SERVING, grpc_health_get_status(server, "any.Service"));

  grpc_health_set_status(server, nullptr, GRPC_HEALTH_NOT_SERVING);
  TEST_ASSERT_EQUAL_INT(GRPC_HEALTH_NOT_SERVING, grpc_health_get_status(server, nullptr));

  grpc_health_set_status(server, "helloworld.Greeter", GRPC_HEALTH_SERVING);
  grpc_health_set_status(server, "order.OrderService", GRPC_HEALTH_NOT_SERVING);
  TEST_ASSERT_EQUAL_INT(GRPC_HEALTH_SERVING,
                        grpc_health_get_status(server, "helloworld.Greeter"));
  TEST_ASSERT_EQUAL_INT(GRPC_HEALTH_NOT_SERVING,
                        grpc_health_get_status(server, "order.OrderService"));
  grpc_health_set_status(server, "order.OrderService", GRPC_HEALTH_SERVING);
  TEST_ASSERT_EQUAL_INT(GRPC_HEALTH_SERVING,
                        grpc_health_get_status(server, "order.OrderService"));

  grpc_server_free(server);
}

void test_health_is_per_server(void) {
  grpc_server *a = new_server(19998);
  grpc_server *b = new_server(19997);
  TEST_ASSERT_NOT_NULL(a);
  TEST_ASSERT_NOT_NULL(b);
  grpc_health_set_status(a, nullptr, GRPC_HEALTH_NOT_SERVING);
  grpc_health_set_status(b, nullptr, GRPC_HEALTH_SERVING);
  TEST_ASSERT_EQUAL_INT(GRPC_HEALTH_NOT_SERVING, grpc_health_get_status(a, nullptr));
  TEST_ASSERT_EQUAL_INT(GRPC_HEALTH_SERVING, grpc_health_get_status(b, nullptr));
  grpc_server_free(a);
  grpc_server_free(b);
}

void test_health_enable_twice(void) {
  grpc_server *server = new_server(19996);
  TEST_ASSERT_NOT_NULL(server);
  TEST_ASSERT_EQUAL_INT(GRPC_OK, grpc_health_enable(server));
  TEST_ASSERT_EQUAL_INT(GRPC_ERR_EXISTS, grpc_health_enable(server));
  TEST_ASSERT_EQUAL_INT(GRPC_ERR_INVALID, grpc_health_enable(nullptr));
  grpc_server_free(server);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_health_status_get_set);
  RUN_TEST(test_health_is_per_server);
  RUN_TEST(test_health_enable_twice);
  return UNITY_END();
}
