#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdint.h>
#include "ezgrpc2.h"
#include "unity.h"

void setUp(void) {}
void tearDown(void) {}

void test_gzip_compression_roundtrip(void) {
  const char *sample = "Hello World! This is a test message to verify RFC 1952 gzip compression "
                       "and decompression with zlib.";
  size_t sample_len = strlen(sample);

  uint8_t *compressed = nullptr;
  size_t comp_len = 0;
  int rc = ezgrpc2_compress_gzip((const uint8_t *)sample, sample_len, &compressed, &comp_len);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_NOT_NULL(compressed);
  TEST_ASSERT_TRUE(comp_len > 0);

  // Gzip magic header: 0x1f, 0x8b
  TEST_ASSERT_EQUAL_HEX8(0x1f, compressed[0]);
  TEST_ASSERT_EQUAL_HEX8(0x8b, compressed[1]);

  uint8_t *decompressed = nullptr;
  size_t decomp_len = 0;
  rc = ezgrpc2_decompress_gzip(compressed, comp_len, &decompressed, &decomp_len);
  TEST_ASSERT_EQUAL_INT(0, rc);
  TEST_ASSERT_NOT_NULL(decompressed);
  TEST_ASSERT_EQUAL_UINT64(sample_len, decomp_len);
  TEST_ASSERT_EQUAL_MEMORY(sample, decompressed, sample_len);

  ezgrpc2_free(compressed);
  ezgrpc2_free(decompressed);
}

void test_message_compress_decompress(void) {
  const char *msg_text = "Test ezgrpc2_message compression wrappers";
  ezgrpc2_message *orig = ezgrpc2_message_new(0, (void *)msg_text, strlen(msg_text));
  TEST_ASSERT_NOT_NULL(orig);
  TEST_ASSERT_EQUAL_UINT8(0, orig->cflag);

  ezgrpc2_message *comp = ezgrpc2_message_compress_gzip(orig);
  TEST_ASSERT_NOT_NULL(comp);
  TEST_ASSERT_EQUAL_UINT8(1, comp->cflag);

  ezgrpc2_message *decomp = ezgrpc2_message_decompress_gzip(comp);
  TEST_ASSERT_NOT_NULL(decomp);
  TEST_ASSERT_EQUAL_UINT8(0, decomp->cflag);
  TEST_ASSERT_EQUAL_UINT32(orig->len, decomp->len);
  TEST_ASSERT_EQUAL_MEMORY(orig->data, decomp->data, orig->len);

  ezgrpc2_message_free(orig);
  ezgrpc2_message_free(comp);
  ezgrpc2_message_free(decomp);
}

int main(void) {
  UNITY_BEGIN();
  RUN_TEST(test_gzip_compression_roundtrip);
  RUN_TEST(test_message_compress_decompress);
  return UNITY_END();
}
