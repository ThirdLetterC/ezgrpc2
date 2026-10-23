#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "grpc.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_status_details_roundtrip_with_any(void) {
  const char *msg = "Resource not found in database";
  const char *type_url = "type.googleapis.com/google.rpc.ErrorInfo";
  const uint8_t raw_payload[] = {0x08, 0x2A, 0x12, 0x04,
                                 0x74, 0x65, 0x73, 0x74}; // Sample inner proto bytes

  grpc_status_detail_entry detail = {
      .type_url = type_url,
      .value = raw_payload,
      .value_len = sizeof(raw_payload),
  };

  size_t enc_len = 0;
  char *b64 = grpc_status_details_encode((int32_t)GRPC_STATUS_NOT_FOUND, msg, &detail, 1,
                                            &enc_len);

  TEST_ASSERT_NOT_NULL(b64);
  TEST_ASSERT_TRUE(enc_len > 0);

  int32_t dec_code = 0;
  char *dec_msg = nullptr;
  char *dec_url = nullptr;
  uint8_t *dec_val = nullptr;
  size_t dec_vlen = 0;

  int rc = grpc_status_details_decode(b64, &dec_code, &dec_msg, &dec_url, &dec_val, &dec_vlen);

  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_INT32((int32_t)GRPC_STATUS_NOT_FOUND, dec_code);
  TEST_ASSERT_NOT_NULL(dec_msg);
  TEST_ASSERT_EQUAL_STRING(msg, dec_msg);
  TEST_ASSERT_NOT_NULL(dec_url);
  TEST_ASSERT_EQUAL_STRING(type_url, dec_url);
  TEST_ASSERT_EQUAL_UINT64(sizeof(raw_payload), dec_vlen);
  TEST_ASSERT_EQUAL_MEMORY(raw_payload, dec_val, sizeof(raw_payload));

  grpc_free(b64);
  grpc_free(dec_msg);
  grpc_free(dec_url);
  grpc_free(dec_val);
}

void test_status_details_basic_roundtrip(void) {
  const char *msg = "Invalid user argument";
  size_t enc_len = 0;
  char *b64 = grpc_status_details_encode((int32_t)GRPC_STATUS_INVALID_ARGUMENT, msg,
                                            nullptr, 0, &enc_len);

  TEST_ASSERT_NOT_NULL(b64);

  int32_t dec_code = 0;
  char *dec_msg = nullptr;

  int rc = grpc_status_details_decode(b64, &dec_code, &dec_msg, nullptr, nullptr, nullptr);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_EQUAL_INT32((int32_t)GRPC_STATUS_INVALID_ARGUMENT, dec_code);
  TEST_ASSERT_NOT_NULL(dec_msg);
  TEST_ASSERT_EQUAL_STRING(msg, dec_msg);

  grpc_free(b64);
  grpc_free(dec_msg);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_status_details_roundtrip_with_any);
  RUN_TEST(test_status_details_basic_roundtrip);
  return UNITY_END();
}
