#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "ezgrpc2.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_list_message_operations(void) {
  ezgrpc2_list_message_t *list = ezgrpc2_list_message_new(nullptr);
  TEST_ASSERT_NOT_NULL(list);
  TEST_ASSERT_TRUE(ezgrpc2_list_message_is_empty(list));
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_message_count(list));

  // Push back
  ezgrpc2_message *m1 = ezgrpc2_message_new(0, (void *)"msg1", 5);
  ezgrpc2_message *m2 = ezgrpc2_message_new(0, (void *)"msg2", 5);
  TEST_ASSERT_EQUAL_INT(0, ezgrpc2_list_message_push_back(list, m1));
  TEST_ASSERT_EQUAL_INT(0, ezgrpc2_list_message_push_back(list, m2));
  TEST_ASSERT_EQUAL_UINT64(2, (uint64_t)ezgrpc2_list_message_count(list));
  TEST_ASSERT_FALSE(ezgrpc2_list_message_is_empty(list));

  // Peek front & back
  TEST_ASSERT_EQUAL_PTR(m1, ezgrpc2_list_message_peek_front(list));
  TEST_ASSERT_EQUAL_PTR(m2, ezgrpc2_list_message_peek_back(list));

  // Pop front
  ezgrpc2_message *popped1 = ezgrpc2_list_message_pop_front(list);
  TEST_ASSERT_EQUAL_PTR(m1, popped1);
  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)ezgrpc2_list_message_count(list));
  ezgrpc2_message_free(popped1);

  // Push front
  ezgrpc2_message *m0 = ezgrpc2_message_new(0, (void *)"msg0", 5);
  TEST_ASSERT_EQUAL_INT(0, ezgrpc2_list_message_push_front(list, m0));
  TEST_ASSERT_EQUAL_PTR(m0, ezgrpc2_list_message_peek_front(list));

  // Pop back
  ezgrpc2_message *popped2 = ezgrpc2_list_message_pop_back(list);
  TEST_ASSERT_EQUAL_PTR(m2, popped2);
  ezgrpc2_message_free(popped2);

  // Concat test
  ezgrpc2_list_message_t *list2 = ezgrpc2_list_message_new(nullptr);
  ezgrpc2_message *m3 = ezgrpc2_message_new(0, (void *)"msg3", 5);
  ezgrpc2_list_message_push_back(list2, m3);
  ezgrpc2_list_message_concat_and_empty_src(list, list2);

  TEST_ASSERT_EQUAL_UINT64(2, (uint64_t)ezgrpc2_list_message_count(list));
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_message_count(list2));

  ezgrpc2_list_message_free(list2);
  ezgrpc2_list_message_free(list);
}

void test_list_event_operations(void) {
  ezgrpc2_list_event_t *list = ezgrpc2_list_event_new(nullptr);
  TEST_ASSERT_NOT_NULL(list);
  TEST_ASSERT_TRUE(ezgrpc2_list_event_is_empty(list));
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_event_count(list));

  ezgrpc2_event *e1 = malloc(sizeof(*e1));
  memset(e1, 0, sizeof(*e1));
  e1->type = EZGRPC2_EVENT_CONNECT;

  ezgrpc2_event *e2 = malloc(sizeof(*e2));
  memset(e2, 0, sizeof(*e2));
  e2->type = EZGRPC2_EVENT_DISCONNECT;

  TEST_ASSERT_EQUAL_INT(0, ezgrpc2_list_event_push_back(list, e1));
  TEST_ASSERT_EQUAL_INT(0, ezgrpc2_list_event_push_back(list, e2));
  TEST_ASSERT_EQUAL_UINT64(2, (uint64_t)ezgrpc2_list_event_count(list));

  TEST_ASSERT_EQUAL_PTR(e1, ezgrpc2_list_event_peek_front(list));
  TEST_ASSERT_EQUAL_PTR(e2, ezgrpc2_list_event_peek_back(list));

  ezgrpc2_event *popped = ezgrpc2_list_event_pop_front(list);
  TEST_ASSERT_EQUAL_PTR(e1, popped);
  TEST_ASSERT_EQUAL_INT(EZGRPC2_EVENT_CONNECT, popped->type);
  free(popped);

  TEST_ASSERT_EQUAL_UINT64(1, (uint64_t)ezgrpc2_list_event_count(list));

  // Concat test
  ezgrpc2_list_event_t *list2 = ezgrpc2_list_event_new(nullptr);
  ezgrpc2_event *e3 = malloc(sizeof(*e3));
  memset(e3, 0, sizeof(*e3));
  e3->type = EZGRPC2_EVENT_CANCEL;
  ezgrpc2_list_event_push_back(list2, e3);

  ezgrpc2_list_event_concat_and_empty_src(list, list2);
  TEST_ASSERT_EQUAL_UINT64(2, (uint64_t)ezgrpc2_list_event_count(list));
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)ezgrpc2_list_event_count(list2));

  ezgrpc2_list_event_free(list2);
  ezgrpc2_list_event_free(list);
}

void test_http2_settings_api(void) {
  ezgrpc2_http2_settings *settings = ezgrpc2_http2_settings_new(nullptr);
  TEST_ASSERT_NOT_NULL(settings);

  // Test defaults
  TEST_ASSERT_EQUAL_UINT64(1 << 20, (uint64_t)ezgrpc2_http2_settings_get_initial_window_size(settings));
  TEST_ASSERT_EQUAL_UINT64(16 * 1024, (uint64_t)ezgrpc2_http2_settings_get_max_frame_size(settings));
  TEST_ASSERT_EQUAL_UINT64(1024, (uint64_t)ezgrpc2_http2_settings_get_max_concurrent_streams(settings));
  TEST_ASSERT_EQUAL_UINT64(1024, (uint64_t)ezgrpc2_http2_settings_get_max_sessions(settings));
  TEST_ASSERT_EQUAL_UINT64(10, (uint64_t)ezgrpc2_http2_settings_get_max_paths(settings));

  // Test setters and getters
  ezgrpc2_http2_settings_set_initial_window_size(settings, 65535);
  TEST_ASSERT_EQUAL_UINT64(65535, (uint64_t)ezgrpc2_http2_settings_get_initial_window_size(settings));

  ezgrpc2_http2_settings_set_max_frame_size(settings, 32768);
  TEST_ASSERT_EQUAL_UINT64(32768, (uint64_t)ezgrpc2_http2_settings_get_max_frame_size(settings));

  ezgrpc2_http2_settings_set_max_concurrent_streams(settings, 500);
  TEST_ASSERT_EQUAL_UINT64(500, (uint64_t)ezgrpc2_http2_settings_get_max_concurrent_streams(settings));

  ezgrpc2_http2_settings_set_max_sessions(settings, 2048);
  TEST_ASSERT_EQUAL_UINT64(2048, (uint64_t)ezgrpc2_http2_settings_get_max_sessions(settings));

  ezgrpc2_http2_settings_set_max_paths(settings, 50);
  TEST_ASSERT_EQUAL_UINT64(50, (uint64_t)ezgrpc2_http2_settings_get_max_paths(settings));

  // Test copy
  ezgrpc2_http2_settings *copy = ezgrpc2_http2_settings_new(nullptr);
  ezgrpc2_http2_settings_copy(copy, settings);
  TEST_ASSERT_EQUAL_UINT64(65535, (uint64_t)ezgrpc2_http2_settings_get_initial_window_size(copy));
  TEST_ASSERT_EQUAL_UINT64(32768, (uint64_t)ezgrpc2_http2_settings_get_max_frame_size(copy));
  TEST_ASSERT_EQUAL_UINT64(500, (uint64_t)ezgrpc2_http2_settings_get_max_concurrent_streams(copy));
  TEST_ASSERT_EQUAL_UINT64(2048, (uint64_t)ezgrpc2_http2_settings_get_max_sessions(copy));
  TEST_ASSERT_EQUAL_UINT64(50, (uint64_t)ezgrpc2_http2_settings_get_max_paths(copy));

  ezgrpc2_http2_settings_free(copy);
  ezgrpc2_http2_settings_free(settings);
}

void test_server_settings_api(void) {
  ezgrpc2_server_settings *settings = ezgrpc2_server_settings_new(nullptr);
  TEST_ASSERT_NOT_NULL(settings);

  // Test defaults
  TEST_ASSERT_EQUAL_UINT64(1024, (uint64_t)ezgrpc2_server_settings_get_max_connections(settings));
  TEST_ASSERT_EQUAL_UINT32(0, ezgrpc2_server_settings_get_log_level(settings));
  TEST_ASSERT_EQUAL_PTR(stderr, ezgrpc2_server_settings_get_log_fp(settings));

  // Test setters and getters
  ezgrpc2_server_settings_set_max_connections(settings, 256);
  TEST_ASSERT_EQUAL_UINT64(256, (uint64_t)ezgrpc2_server_settings_get_max_connections(settings));

  ezgrpc2_server_settings_set_log_level(settings, EZGRPC2_SERVER_LOG_DEBUG);
  TEST_ASSERT_EQUAL_UINT32(EZGRPC2_SERVER_LOG_DEBUG, ezgrpc2_server_settings_get_log_level(settings));

  ezgrpc2_server_settings_set_log_fp(settings, stdout);
  TEST_ASSERT_EQUAL_PTR(stdout, ezgrpc2_server_settings_get_log_fp(settings));

  ezgrpc2_server_settings_set_arena_events_size(settings, 8192);
  TEST_ASSERT_EQUAL_UINT64(8192, (uint64_t)ezgrpc2_server_settings_get_arena_events_size(settings));

  ezgrpc2_server_settings_set_arena_messages_size(settings, 65536);
  TEST_ASSERT_EQUAL_UINT64(65536, (uint64_t)ezgrpc2_server_settings_get_arena_messages_size(settings));

  // Test copy
  ezgrpc2_server_settings *copy = ezgrpc2_server_settings_new(nullptr);
  ezgrpc2_server_settings_copy(copy, settings);
  TEST_ASSERT_EQUAL_UINT64(256, (uint64_t)ezgrpc2_server_settings_get_max_connections(copy));
  TEST_ASSERT_EQUAL_UINT32(EZGRPC2_SERVER_LOG_DEBUG, ezgrpc2_server_settings_get_log_level(copy));
  TEST_ASSERT_EQUAL_PTR(stdout, ezgrpc2_server_settings_get_log_fp(copy));
  TEST_ASSERT_EQUAL_UINT64(8192, (uint64_t)ezgrpc2_server_settings_get_arena_events_size(copy));
  TEST_ASSERT_EQUAL_UINT64(65536, (uint64_t)ezgrpc2_server_settings_get_arena_messages_size(copy));

  ezgrpc2_server_settings_free(copy);
  ezgrpc2_server_settings_free(settings);
}

void test_server_settings_integration(void) {
  ezgrpc2_http2_settings *h2_set = ezgrpc2_http2_settings_new(nullptr);
  ezgrpc2_http2_settings_set_max_concurrent_streams(h2_set, 128);

  ezgrpc2_server_settings *srv_set = ezgrpc2_server_settings_new(nullptr);
  ezgrpc2_server_settings_set_max_connections(srv_set, 32);

  ezgrpc2_server *server = ezgrpc2_server_new("0.0.0.0", 19010, "::", 19010, 16, srv_set, h2_set);
  TEST_ASSERT_NOT_NULL(server);

  ezgrpc2_server_settings *active_srv_set = ezgrpc2_server_get_server_settings(server);
  TEST_ASSERT_NOT_NULL(active_srv_set);
  TEST_ASSERT_EQUAL_UINT64(32, (uint64_t)ezgrpc2_server_settings_get_max_connections(active_srv_set));

  ezgrpc2_http2_settings *active_h2_set = ezgrpc2_server_get_http2_settings(server);
  TEST_ASSERT_NOT_NULL(active_h2_set);
  TEST_ASSERT_EQUAL_UINT64(128, (uint64_t)ezgrpc2_http2_settings_get_max_concurrent_streams(active_h2_set));

  ezgrpc2_server_free(server);
  ezgrpc2_server_settings_free(srv_set);
  ezgrpc2_http2_settings_free(h2_set);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_list_message_operations);
  RUN_TEST(test_list_event_operations);
  RUN_TEST(test_http2_settings_api);
  RUN_TEST(test_server_settings_api);
  RUN_TEST(test_server_settings_integration);
  return UNITY_END();
}
