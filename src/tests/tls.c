#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grpc.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static grpc_server *new_tls_server(const char *cert, const char *key) {
  return grpc_server_new(&(grpc_config){
      .port = 19995,
      .ipv4_addr = "127.0.0.1",
      .tls = {.cert_file = cert, .key_file = key},
      .log_level = GRPC_LOG_OFF,
  });
}

void test_tls_backend(void) {
  /* the library must be built with the backend the build asked for */
  TEST_ASSERT_EQUAL_STRING(EXPECTED_TLS_BACKEND, grpc_tls_backend());
  TEST_ASSERT_TRUE(strlen(grpc_tls_backend_version()) > 0);
  printf("TLS backend: %s (%s)\n", grpc_tls_backend(), grpc_tls_backend_version());
}

void test_tls_invalid_files(void) {
  TEST_ASSERT_NULL(new_tls_server("nonexistent_cert.pem", "nonexistent_key.pem"));
  /* a key that does not belong to the certificate */
  TEST_ASSERT_NULL(new_tls_server("test_cert.pem", "test_cert.pem"));
}

void test_tls_server(void) {
  grpc_server *server = new_tls_server("test_cert.pem", "test_key.pem");
  TEST_ASSERT_NOT_NULL(server);
  TEST_ASSERT_NOT_NULL(grpc_server_tls_native(server));
  grpc_server_free(server);

  server = grpc_server_new(
      &(grpc_config){.port = 19995, .ipv4_addr = "127.0.0.1", .log_level = GRPC_LOG_OFF});
  TEST_ASSERT_NOT_NULL(server);
  TEST_ASSERT_NULL(grpc_server_tls_native(server));
  grpc_server_free(server);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_tls_backend);
  RUN_TEST(test_tls_invalid_files);
  RUN_TEST(test_tls_server);
  return UNITY_END();
}
