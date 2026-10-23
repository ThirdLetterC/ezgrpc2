#include <pthread.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "ezgrpc2.h"
#include "unity.h"

/* the public headers must not leak macros into the application */
#if defined(malloc) || defined(free) || defined(calloc) || defined(realloc) || defined(strdup)
#error "ezgrpc2.h redefines allocation functions"
#endif
#if defined(likely) || defined(unlikely) || defined(REALIGN) || defined(COLSTR) ||                 \
    defined(LOG_ERROR) || defined(CONFIG_STREAM_MAX_HEADERS)
#error "ezgrpc2.h leaks internal macros"
#endif

#include "core.h"

void setUp(void) {}
void tearDown(void) {}

#define PORT 19990

static void handler([[maybe_unused]] ezgrpc2_call *call,
                    [[maybe_unused]] const ezgrpc2_message *request,
                    [[maybe_unused]] void *userdata) {}

static void on_message([[maybe_unused]] ezgrpc2_call *call,
                       [[maybe_unused]] const ezgrpc2_message *message,
                       [[maybe_unused]] void *userdata) {}

void test_config_validation(void) {
  /* a port is required */
  TEST_ASSERT_NULL(ezgrpc2_server_new(nullptr));
  TEST_ASSERT_NULL(ezgrpc2_server_new(&(ezgrpc2_config){.log_level = EZGRPC2_LOG_OFF}));
  /* invalid addresses and limits */
  TEST_ASSERT_NULL(ezgrpc2_server_new(&(ezgrpc2_config){
      .port = PORT, .ipv4_addr = "not an address", .log_level = EZGRPC2_LOG_OFF}));
  TEST_ASSERT_NULL(ezgrpc2_server_new(&(ezgrpc2_config){
      .port = PORT, .max_frame_size = 100, .log_level = EZGRPC2_LOG_OFF}));
  /* TLS needs both files */
  TEST_ASSERT_NULL(ezgrpc2_server_new(&(ezgrpc2_config){
      .port = PORT, .tls.cert_file = "cert.pem", .log_level = EZGRPC2_LOG_OFF}));

  /* only the given address family */
  ezgrpc2_server *server = ezgrpc2_server_new(
      &(ezgrpc2_config){.port = PORT, .ipv4_addr = "127.0.0.1", .log_level = EZGRPC2_LOG_OFF});
  TEST_ASSERT_NOT_NULL(server);
  ezgrpc2_server_free(server);

  ezgrpc2_server_free(nullptr);
}

void test_routes(void) {
  ezgrpc2_server *server = ezgrpc2_server_new(
      &(ezgrpc2_config){.port = PORT, .ipv4_addr = "127.0.0.1", .log_level = EZGRPC2_LOG_OFF});
  TEST_ASSERT_NOT_NULL(server);

  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_server_handle(server, "/a.B/C", handler, nullptr));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_EXISTS,
                        ezgrpc2_server_handle(server, "/a.B/C", handler, nullptr));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID,
                        ezgrpc2_server_handle(server, "no-slash", handler, nullptr));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID,
                        ezgrpc2_server_handle(server, "/a.B/D", nullptr, nullptr));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID,
                        ezgrpc2_server_handle_stream(server, "/a.B/D",
                                                     &(ezgrpc2_stream_handler){}, nullptr));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK,
                        ezgrpc2_server_handle_stream(
                            server, "/a.B/D", &(ezgrpc2_stream_handler){.on_message = on_message},
                            nullptr));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_server_register_path(server, "/a.B/E", nullptr));

  /* more methods than the old fixed limit of 10 */
  for (int i = 0; i < 100; i++) {
    char path[32];
    snprintf(path, sizeof(path), "/many.S/M%d", i);
    TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_server_handle(server, path, handler, nullptr));
  }

  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_server_unregister(server, "/a.B/C"));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_NOT_FOUND, ezgrpc2_server_unregister(server, "/a.B/C"));
  /* a disabled path can be registered again */
  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_server_handle(server, "/a.B/C", handler, nullptr));

  ezgrpc2_server_free(server);
}

void test_poll_idle(void) {
  ezgrpc2_server *server = ezgrpc2_server_new(
      &(ezgrpc2_config){.port = PORT, .ipv4_addr = "127.0.0.1", .log_level = EZGRPC2_LOG_OFF});
  TEST_ASSERT_NOT_NULL(server);
  TEST_ASSERT_EQUAL_INT(0, ezgrpc2_server_poll(server, 0));
  TEST_ASSERT_EQUAL_INT(0, ezgrpc2_server_poll(server, 10));
  size_t count = 1;
  ezgrpc2_server_events(server, &count);
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)count);
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID, ezgrpc2_server_poll(nullptr, 0));
  ezgrpc2_server_free(server);
}

typedef struct thread_arg {
  ezgrpc2_server *server;
  int *counter;
} thread_arg;

static void count(void *arg) {
  (*(int *)arg)++;
}

static void *post_and_stop(void *arg) {
  thread_arg *a = arg;
  for (int i = 0; i < 100; i++)
    ezgrpc2_server_post(a->server, count, a->counter);
  ezgrpc2_server_stop(a->server);
  return nullptr;
}

void test_post_and_stop_from_another_thread(void) {
  ezgrpc2_server *server = ezgrpc2_server_new(
      &(ezgrpc2_config){.port = PORT, .ipv4_addr = "127.0.0.1", .log_level = EZGRPC2_LOG_OFF});
  TEST_ASSERT_NOT_NULL(server);

  int counter = 0;
  thread_arg arg = {server, &counter};
  pthread_t thread;
  pthread_create(&thread, nullptr, post_and_stop, &arg);
  /* returns once the other thread stops it */
  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_server_run(server));
  pthread_join(thread, nullptr);
  /* run on the server thread: whatever ran before the stop, the rest at free */
  ezgrpc2_server_free(server);
  TEST_ASSERT_EQUAL_INT(100, counter);
}

static size_t read_log(FILE *file, char *buffer, size_t size) {
  fflush(file);
  rewind(file);
  size_t n = fread(buffer, 1, size - 1, file);
  buffer[n] = '\0';
  return n;
}

void test_log_level_control(void) {
  FILE *first_log = tmpfile();
  FILE *second_log = tmpfile();
  TEST_ASSERT_NOT_NULL(first_log);
  TEST_ASSERT_NOT_NULL(second_log);

  ezgrpc2_server *first = ezgrpc2_server_new(&(ezgrpc2_config){.port = PORT,
                                                               .ipv4_addr = "127.0.0.1",
                                                               .log_level = EZGRPC2_LOG_WARNING,
                                                               .log_file = first_log});
  TEST_ASSERT_NOT_NULL(first);
  ezgrpc2_server *second = ezgrpc2_server_new(&(ezgrpc2_config){.port = PORT,
                                                                .ipv4_addr = "127.0.0.1",
                                                                .log_level = EZGRPC2_LOG_ERROR,
                                                                .log_file = second_log});
  TEST_ASSERT_NOT_NULL(second);

  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID,
                        ezgrpc2_server_set_log_level(nullptr, EZGRPC2_LOG_INFO));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID,
                        ezgrpc2_server_set_log_level(first, (ezgrpc2_log_level)-1));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID,
                        ezgrpc2_server_set_log_level(first, (ezgrpc2_log_level)7));

  server_log(first, EZGRPC2_LOG_INFO, "filtered-first\n");
  server_log(first, EZGRPC2_LOG_WARNING, "warning-first\n");
  server_log(second, EZGRPC2_LOG_WARNING, "filtered-second\n");
  server_log(second, EZGRPC2_LOG_ERROR, "error-second\n");
  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_server_set_log_level(first, EZGRPC2_LOG_DEBUG));
  server_log(first, EZGRPC2_LOG_DEBUG, "debug-first\n");
  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_server_set_log_level(second, EZGRPC2_LOG_OFF));
  server_log(second, EZGRPC2_LOG_ERROR, "filtered-off\n");

  char first_contents[4096], second_contents[4096];
  read_log(first_log, first_contents, sizeof(first_contents));
  read_log(second_log, second_contents, sizeof(second_contents));
  TEST_ASSERT_NOT_NULL(strstr(first_contents, "warning-first"));
  TEST_ASSERT_NOT_NULL(strstr(first_contents, "debug-first"));
  TEST_ASSERT_NULL(strstr(first_contents, "filtered-first"));
  TEST_ASSERT_NULL(strstr(first_contents, "error-second"));
  TEST_ASSERT_NOT_NULL(strstr(second_contents, "error-second"));
  TEST_ASSERT_NULL(strstr(second_contents, "filtered-second"));
  TEST_ASSERT_NULL(strstr(second_contents, "filtered-off"));

  ezgrpc2_server_free(first);
  ezgrpc2_server_free(second);
  fclose(first_log);
  fclose(second_log);
}

void test_strings(void) {
  TEST_ASSERT_EQUAL_STRING("success", ezgrpc2_strerror(EZGRPC2_OK));
  TEST_ASSERT_EQUAL_STRING("already exists", ezgrpc2_strerror(EZGRPC2_ERR_EXISTS));
  TEST_ASSERT_EQUAL_STRING("unknown error", ezgrpc2_strerror((ezgrpc2_err)-1000));
  TEST_ASSERT_EQUAL_STRING("invalid argument", ezgrpc2_status_str(EZGRPC2_STATUS_INVALID_ARGUMENT));
  TEST_ASSERT_EQUAL_STRING("aborted", ezgrpc2_status_str(EZGRPC2_STATUS_ABORTED));
  TEST_ASSERT_NOT_NULL(ezgrpc2_license());
}

void test_messages(void) {
  ezgrpc2_message *msg = ezgrpc2_message_new("hello", 5);
  TEST_ASSERT_NOT_NULL(msg);
  TEST_ASSERT_EQUAL_UINT32(5, msg->len);
  TEST_ASSERT_EQUAL_UINT8(0, msg->cflag);
  TEST_ASSERT_EQUAL_MEMORY("hello", msg->data, 5);
  TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)msg->data % alignof(max_align_t));
  ezgrpc2_message_free(msg);

  msg = ezgrpc2_message_alloc(1000);
  TEST_ASSERT_NOT_NULL(msg);
  TEST_ASSERT_EQUAL_UINT32(1000, msg->len);
  memset(msg->data, 0xab, 1000);
  ezgrpc2_message_free(msg);

  TEST_ASSERT_NULL(ezgrpc2_message_new(nullptr, 5));
  msg = ezgrpc2_message_new(nullptr, 0);
  TEST_ASSERT_NOT_NULL(msg);
  ezgrpc2_message_free(msg);
  ezgrpc2_message_free(nullptr);
}

void test_call_null_safety(void) {
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID, ezgrpc2_call_finish(nullptr, EZGRPC2_STATUS_OK, nullptr));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID, ezgrpc2_call_send(nullptr, nullptr));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID, ezgrpc2_call_send_data(nullptr, "x", 1));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID, ezgrpc2_call_send_async(nullptr, nullptr));
  TEST_ASSERT_NULL(ezgrpc2_call_metadata(nullptr, "key", nullptr));
  TEST_ASSERT_EQUAL_INT64(-1, ezgrpc2_call_deadline(nullptr));
  TEST_ASSERT_FALSE(ezgrpc2_call_is_cancelled(nullptr));
  ezgrpc2_call_unref(nullptr);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_config_validation);
  RUN_TEST(test_routes);
  RUN_TEST(test_poll_idle);
  RUN_TEST(test_post_and_stop_from_another_thread);
  RUN_TEST(test_log_level_control);
  RUN_TEST(test_strings);
  RUN_TEST(test_messages);
  RUN_TEST(test_call_null_safety);
  return UNITY_END();
}
