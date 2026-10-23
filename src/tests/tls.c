#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ezgrpc2.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_tls_invalid_args(void) {
  TEST_ASSERT_NULL(ezgrpc2_tls_init_server_ctx(nullptr, nullptr));
  TEST_ASSERT_NULL(ezgrpc2_tls_init_server_ctx("nonexistent_cert.pem", "nonexistent_key.pem"));
  TEST_ASSERT_EQUAL_INT(1, ezgrpc2_server_enable_tls(nullptr, "cert.pem", "key.pem"));
}

void test_tls_init_and_enable(void) {
  SSL_CTX *ctx = ezgrpc2_tls_init_server_ctx("test_cert.pem", "test_key.pem");
  TEST_ASSERT_NOT_NULL(ctx);
  ezgrpc2_tls_free_server_ctx(ctx);

  ezgrpc2_server *server = ezgrpc2_server_new("0.0.0.0", 19996, nullptr, 0, 16, nullptr, nullptr);
  TEST_ASSERT_NOT_NULL(server);

  int rc = ezgrpc2_server_enable_tls(server, "test_cert.pem", "test_key.pem");
  TEST_ASSERT_EQUAL_INT(0, rc);

  ezgrpc2_server_free(server);
}

void test_server_new_tls(void) {
  ezgrpc2_server *server = ezgrpc2_server_new_tls("0.0.0.0", 19995, nullptr, 0, 16, "test_cert.pem",
                                                  "test_key.pem", nullptr, nullptr);
  TEST_ASSERT_NOT_NULL(server);

  ezgrpc2_server_free(server);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tls_invalid_args);
  RUN_TEST(test_tls_init_and_enable);
  RUN_TEST(test_server_new_tls);
  return UNITY_END();
}
