#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ezgrpc2.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static ezgrpc2_server *new_tls_server(const char *cert, const char *key) {
  return ezgrpc2_server_new(&(ezgrpc2_config){
      .port = 19995,
      .ipv4_addr = "127.0.0.1",
      .tls = {.cert_file = cert, .key_file = key},
      .log_level = EZGRPC2_LOG_OFF,
  });
}

void test_tls_backend(void) {
  /* the library must be built with the backend the build asked for */
  TEST_ASSERT_EQUAL_STRING(EXPECTED_TLS_BACKEND, ezgrpc2_tls_backend());
  TEST_ASSERT_TRUE(strlen(ezgrpc2_tls_backend_version()) > 0);
  printf("TLS backend: %s (%s)\n", ezgrpc2_tls_backend(), ezgrpc2_tls_backend_version());
}

void test_tls_invalid_files(void) {
  TEST_ASSERT_NULL(new_tls_server("nonexistent_cert.pem", "nonexistent_key.pem"));
  /* a key that does not belong to the certificate */
  TEST_ASSERT_NULL(new_tls_server("test_cert.pem", "test_cert.pem"));
}

void test_tls_server(void) {
  ezgrpc2_server *server = new_tls_server("test_cert.pem", "test_key.pem");
  TEST_ASSERT_NOT_NULL(server);
  TEST_ASSERT_NOT_NULL(ezgrpc2_server_tls_native(server));
  ezgrpc2_server_free(server);

  server = ezgrpc2_server_new(
      &(ezgrpc2_config){.port = 19995, .ipv4_addr = "127.0.0.1", .log_level = EZGRPC2_LOG_OFF});
  TEST_ASSERT_NOT_NULL(server);
  TEST_ASSERT_NULL(ezgrpc2_server_tls_native(server));
  ezgrpc2_server_free(server);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tls_backend);
  RUN_TEST(test_tls_invalid_files);
  RUN_TEST(test_tls_server);
  return UNITY_END();
}
