#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "grpc.h"
#include "unity.h"

/* interceptors run on real calls; the Python integration test of
 * examples/python_client_c_server exercises on_call, rejection and on_done */

void setUp(void) {}
void tearDown(void) {}

static int s_freed = 0;

static grpc_status pass([[maybe_unused]] grpc_call *call,
                           [[maybe_unused]] const char **message,
                           [[maybe_unused]] void *userdata) {
  return GRPC_STATUS_OK;
}

static void count_free([[maybe_unused]] void *userdata) {
  s_freed++;
}

static bool validator(const char *token, [[maybe_unused]] void *userdata) {
  return strcmp(token, "valid-secret-token") == 0;
}

void test_interceptor_registration(void) {
  grpc_server *server = grpc_server_new(
      &(grpc_config){.port = 19998, .ipv4_addr = "127.0.0.1", .log_level = GRPC_LOG_OFF});
  TEST_ASSERT_NOT_NULL(server);

  grpc_interceptor inc = {
      .name = "pass",
      .on_call = pass,
      .userdata = &s_freed,
      .userdata_free = count_free,
  };
  TEST_ASSERT_EQUAL_INT(GRPC_OK, grpc_server_add_interceptor(server, &inc));
  TEST_ASSERT_EQUAL_INT(GRPC_OK, grpc_server_add_interceptor(server, &inc));
  TEST_ASSERT_EQUAL_INT(GRPC_ERR_INVALID, grpc_server_add_interceptor(server, nullptr));

  /* the server owns the userdata */
  s_freed = 0;
  grpc_server_free(server);
  TEST_ASSERT_EQUAL_INT(2, s_freed);
}

void test_bearer_auth_interceptor(void) {
  grpc_interceptor auth = grpc_interceptor_bearer_auth(validator, nullptr);
  TEST_ASSERT_EQUAL_STRING("bearer_auth", auth.name);
  TEST_ASSERT_NOT_NULL(auth.on_call);
  TEST_ASSERT_NOT_NULL(auth.userdata);
  TEST_ASSERT_NOT_NULL(auth.userdata_free);

  /* freed with the server */
  grpc_server *server = grpc_server_new(
      &(grpc_config){.port = 19998, .ipv4_addr = "127.0.0.1", .log_level = GRPC_LOG_OFF});
  TEST_ASSERT_NOT_NULL(server);
  TEST_ASSERT_EQUAL_INT(GRPC_OK, grpc_server_add_interceptor(server, &auth));
  grpc_server_free(server);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_interceptor_registration);
  RUN_TEST(test_bearer_auth_interceptor);
  return UNITY_END();
}
