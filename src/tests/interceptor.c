#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ezgrpc2.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static int s_pre_calls = 0;
static int s_post_calls = 0;

static ezgrpc2_grpc_status dummy_pre_pass([[maybe_unused]] ezgrpc2_server *server,
                                          [[maybe_unused]] const ezgrpc2_interceptor_info *info,
                                          [[maybe_unused]] const char **custom_error_msg,
                                          [[maybe_unused]] void *interceptor_data) {
  s_pre_calls++;
  return EZGRPC2_GRPC_STATUS_OK;
}

static ezgrpc2_grpc_status dummy_pre_reject([[maybe_unused]] ezgrpc2_server *server,
                                            [[maybe_unused]] const ezgrpc2_interceptor_info *info,
                                            const char **custom_error_msg,
                                            [[maybe_unused]] void *interceptor_data) {
  s_pre_calls++;
  *custom_error_msg = "Access Denied by Interceptor";
  return EZGRPC2_GRPC_STATUS_PERMISSION_DENIED;
}

static void dummy_post([[maybe_unused]] ezgrpc2_server *server,
                       [[maybe_unused]] const ezgrpc2_interceptor_info *info,
                       [[maybe_unused]] ezgrpc2_grpc_status status,
                       [[maybe_unused]] uint64_t duration_ms,
                       [[maybe_unused]] void *interceptor_data) {
  s_post_calls++;
}

static bool my_token_validator(const char *token, [[maybe_unused]] void *user_data) {
  return strcmp(token, "valid-secret-token") == 0;
}

void test_interceptor_registration_and_hooks(void) {
  ezgrpc2_server *server = ezgrpc2_server_new("0.0.0.0", 19998, nullptr, 0, 16, nullptr, nullptr);
  TEST_ASSERT_NOT_NULL(server);

  ezgrpc2_interceptor pass_inc = {
      .name = "pass_inc",
      .pre_fn = dummy_pre_pass,
      .post_fn = dummy_post,
      .user_data = nullptr,
  };
  TEST_ASSERT_EQUAL_INT(0, ezgrpc2_server_add_interceptor(server, &pass_inc));

  s_pre_calls = 0;
  s_post_calls = 0;

  ezgrpc2_event dummy_event = {};
  dummy_event.type = EZGRPC2_EVENT_MESSAGE;
  dummy_event.session_uuid = (ezgrpc2_session_uuid){0};
  dummy_event.message.stream_id = 1;
  dummy_event.message.end_stream = true;

  bool ok = ezgrpc2_server_intercept_pre(server, &dummy_event, "/test.Service/Method");
  TEST_ASSERT_TRUE(ok);
  TEST_ASSERT_EQUAL_INT(1, s_pre_calls);

  ezgrpc2_server_intercept_post(server, &dummy_event, "/test.Service/Method",
                                EZGRPC2_GRPC_STATUS_OK, 10);
  TEST_ASSERT_EQUAL_INT(1, s_post_calls);

  // Add reject interceptor
  ezgrpc2_interceptor rej_inc = {
      .name = "rej_inc",
      .pre_fn = dummy_pre_reject,
      .post_fn = nullptr,
      .user_data = nullptr,
  };
  TEST_ASSERT_EQUAL_INT(0, ezgrpc2_server_add_interceptor(server, &rej_inc));

  // Pre intercept should now fail
  ok = ezgrpc2_server_intercept_pre(server, &dummy_event, "/test.Service/Method");
  TEST_ASSERT_FALSE(ok);
  TEST_ASSERT_EQUAL_INT(3, s_pre_calls); // 1 from pass_inc, 1 from rej_inc

  ezgrpc2_server_free(server);
}

void test_bearer_auth_interceptor_creation(void) {
  ezgrpc2_interceptor auth_inc =
      ezgrpc2_interceptor_create_bearer_auth(my_token_validator, nullptr);
  TEST_ASSERT_NOT_NULL(auth_inc.pre_fn);
  TEST_ASSERT_EQUAL_STRING("bearer_auth", auth_inc.name);
  ezgrpc2_free(auth_inc.user_data);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_interceptor_registration_and_hooks);
  RUN_TEST(test_bearer_auth_interceptor_creation);
  return UNITY_END();
}
