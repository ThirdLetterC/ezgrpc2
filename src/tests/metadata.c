#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "grpc.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_metadata_base64_roundtrip(void) {
  const char *test_strings[] = {
      "", "f", "fo", "foo", "foob", "fooba", "foobar", "Hello, gRPC Binary Metadata World!",
  };

  for (size_t i = 0; i < sizeof(test_strings) / sizeof(test_strings[0]); i++) {
    const char *orig = test_strings[i];
    size_t orig_len = strlen(orig);

    size_t enc_len = 0;
    char *encoded = grpc_metadata_encode_bin((const uint8_t *)orig, orig_len, &enc_len);
    TEST_ASSERT_NOT_NULL(encoded);
    TEST_ASSERT_EQUAL_UINT64(enc_len, strlen(encoded));

    size_t dec_len = 0;
    uint8_t *decoded = grpc_metadata_decode_bin(encoded, enc_len, &dec_len);
    TEST_ASSERT_NOT_NULL(decoded);
    TEST_ASSERT_EQUAL_UINT64(orig_len, dec_len);
    if (orig_len > 0) {
      TEST_ASSERT_EQUAL_MEMORY(orig, decoded, orig_len);
    }

    grpc_free(encoded);
    grpc_free(decoded);
  }
}

void test_metadata_binary_random_bytes(void) {
  uint8_t raw[256];
  for (size_t i = 0; i < sizeof(raw); i++) {
    raw[i] = (uint8_t)(i & 0xFF);
  }

  size_t enc_len = 0;
  char *encoded = grpc_metadata_encode_bin(raw, sizeof(raw), &enc_len);
  TEST_ASSERT_NOT_NULL(encoded);

  size_t dec_len = 0;
  uint8_t *decoded = grpc_metadata_decode_bin(encoded, enc_len, &dec_len);
  TEST_ASSERT_NOT_NULL(decoded);
  TEST_ASSERT_EQUAL_UINT64(sizeof(raw), dec_len);
  TEST_ASSERT_EQUAL_MEMORY(raw, decoded, sizeof(raw));

  grpc_free(encoded);
  grpc_free(decoded);
}

void test_metadata_null_handling(void) {
  TEST_ASSERT_NULL(grpc_metadata_encode_bin(nullptr, 0, nullptr));
  TEST_ASSERT_NULL(grpc_metadata_decode_bin(nullptr, 0, nullptr));
  TEST_ASSERT_NULL(grpc_call_metadata(nullptr, "test", nullptr));
  TEST_ASSERT_EQUAL_UINT64(0, (uint64_t)grpc_call_metadata_count(nullptr));
  grpc_metadata entry;
  TEST_ASSERT_FALSE(grpc_call_metadata_at(nullptr, 0, &entry));
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_metadata_base64_roundtrip);
  RUN_TEST(test_metadata_binary_random_bytes);
  RUN_TEST(test_metadata_null_handling);
  return UNITY_END();
}
