#include <stdio.h>
#include <stdlib.h>
#include <stdint.h>
#include <string.h>
#include "ezgrpc2_flatcc.h"
#include "test_msg_builder.h"
#include "test_msg_verifier.h"
#include "test_noid_builder.h"
#include "test_noid_verifier.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

static flatcc_builder_t builder;

static ezgrpc2_message *build_echo(const char *text, int32_t number) {
  test_Echo_start_as_root(&builder);
  test_Echo_text_create_str(&builder, text);
  test_Echo_number_add(&builder, number);
  test_Echo_tags_start(&builder);
  test_Echo_tags_push_create_str(&builder, "a");
  test_Echo_tags_push_create_str(&builder, "b");
  test_Echo_tags_end(&builder);
  test_Echo_end_as_root(&builder);
  return ezgrpc2_fb_pack(&builder);
}

void test_fb_pack_and_unpack(void) {
  ezgrpc2_message *msg = build_echo("Hello from ezgrpc2 flatcc integration!", 42);
  TEST_ASSERT_NOT_NULL(msg);
  TEST_ASSERT_EQUAL_UINT8(0, msg->cflag);
  TEST_ASSERT_GREATER_THAN_UINT32(0, msg->len);
  /* read in place, so the payload must be aligned */
  TEST_ASSERT_EQUAL_UINT64(0, (uintptr_t)msg->data % alignof(max_align_t));

  TEST_ASSERT_EQUAL_INT(flatcc_verify_ok, EZGRPC2_FB_VERIFY(test_Echo, msg));
  test_Echo_table_t echo = EZGRPC2_FB_UNPACK(test_Echo, msg);
  TEST_ASSERT_NOT_NULL(echo);
  TEST_ASSERT_EQUAL_STRING("Hello from ezgrpc2 flatcc integration!", test_Echo_text(echo));
  TEST_ASSERT_EQUAL_INT32(42, test_Echo_number(echo));
  flatbuffers_string_vec_t tags = test_Echo_tags(echo);
  TEST_ASSERT_EQUAL_UINT64(2, (uint64_t)flatbuffers_string_vec_len(tags));
  TEST_ASSERT_EQUAL_STRING("b", flatbuffers_string_vec_at(tags, 1));

  ezgrpc2_message_free(msg);
}

void test_fb_builder_reuse(void) {
  /* ezgrpc2_fb_pack() resets the builder for the next buffer */
  for (int i = 0; i < 100; i++) {
    char text[32];
    snprintf(text, sizeof(text), "message %d", i);
    ezgrpc2_message *msg = build_echo(text, i);
    TEST_ASSERT_NOT_NULL(msg);
    test_Echo_table_t echo = EZGRPC2_FB_UNPACK(test_Echo, msg);
    TEST_ASSERT_NOT_NULL(echo);
    TEST_ASSERT_EQUAL_STRING(text, test_Echo_text(echo));
    TEST_ASSERT_EQUAL_INT32(i, test_Echo_number(echo));
    ezgrpc2_message_free(msg);
  }
}

void test_fb_send_null_safety(void) {
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID, ezgrpc2_call_send_fb(nullptr, &builder));
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID, ezgrpc2_call_send_fb(nullptr, nullptr));
}

void test_fb_rejects_invalid(void) {
  ezgrpc2_message *msg = build_echo("payload", 7);
  TEST_ASSERT_NOT_NULL(msg);

  /* wrong file identifier */
  TEST_ASSERT_NOT_EQUAL(flatcc_verify_ok, ezgrpc2_fb_verify(msg, "XXXX", test_Echo_verify_table));
  TEST_ASSERT_NULL(ezgrpc2_fb_unpack(msg, "XXXX", test_Echo_verify_table));

  /* truncated */
  ezgrpc2_message *cut = ezgrpc2_message_new(msg->data, msg->len / 2);
  TEST_ASSERT_NULL(EZGRPC2_FB_UNPACK(test_Echo, cut));
  ezgrpc2_message_free(cut);

  /* root offset pointing outside the buffer */
  ezgrpc2_message *bad = ezgrpc2_message_new(msg->data, msg->len);
  bad->data[0] = 0xff;
  bad->data[1] = 0xff;
  TEST_ASSERT_NULL(EZGRPC2_FB_UNPACK(test_Echo, bad));
  ezgrpc2_message_free(bad);

  /* not a FlatBuffer at all */
  ezgrpc2_message *junk = ezgrpc2_message_new("junk", 4);
  TEST_ASSERT_NULL(EZGRPC2_FB_UNPACK(test_Echo, junk));
  ezgrpc2_message_free(junk);

  /* empty */
  ezgrpc2_message *empty = ezgrpc2_message_new(nullptr, 0);
  TEST_ASSERT_NULL(EZGRPC2_FB_UNPACK(test_Echo, empty));
  ezgrpc2_message_free(empty);

  ezgrpc2_message_free(msg);
}

void test_fb_without_identifier(void) {
  test_Ping_start_as_root(&builder);
  test_Ping_value_add(&builder, INT64_C(1) << 40);
  test_Ping_end_as_root(&builder);
  ezgrpc2_message *msg = ezgrpc2_fb_pack(&builder);
  TEST_ASSERT_NOT_NULL(msg);

  test_Ping_table_t ping = EZGRPC2_FB_UNPACK(test_Ping, msg);
  TEST_ASSERT_NOT_NULL(ping);
  TEST_ASSERT_EQUAL_INT64(INT64_C(1) << 40, test_Ping_value(ping));
  ezgrpc2_message_free(msg);
}

void test_fb_null_safety(void) {
  TEST_ASSERT_EQUAL_INT(EZGRPC2_ERR_INVALID, ezgrpc2_fb_builder_init(nullptr));
  TEST_ASSERT_NULL(ezgrpc2_fb_pack(nullptr));
  TEST_ASSERT_NULL(ezgrpc2_fb_unpack(nullptr, nullptr, test_Echo_verify_table));
  TEST_ASSERT_NOT_EQUAL(flatcc_verify_ok, ezgrpc2_fb_verify(nullptr, nullptr, nullptr));
  /* nothing was built */
  TEST_ASSERT_NULL(ezgrpc2_fb_pack(&builder));
}

int main(void) {
  TEST_ASSERT_EQUAL_INT(EZGRPC2_OK, ezgrpc2_fb_builder_init(&builder));
  UNITY_BEGIN();
  RUN_TEST(test_fb_pack_and_unpack);
  RUN_TEST(test_fb_builder_reuse);
  RUN_TEST(test_fb_send_null_safety);
  RUN_TEST(test_fb_rejects_invalid);
  RUN_TEST(test_fb_without_identifier);
  RUN_TEST(test_fb_null_safety);
  int res = UNITY_END();
  flatcc_builder_clear(&builder);
  return res;
}
