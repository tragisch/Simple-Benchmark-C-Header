
#include "simple_bench.h"  // Include the header file where format_duration is declared
#include "unity.h"

#define BUFFER_SIZE 50

void setUp() {}

void tearDown() {}

void test_dummy(void) { TEST_ASSERT_EQUAL(1, 1); }

void test_format_duration_nanos(void) {
  char buffer[BUFFER_SIZE];
  format_duration(999LL, buffer, BUFFER_SIZE);
  TEST_ASSERT_EQUAL_STRING("999 ns", buffer);
}

void test_format_duration_micros(void) {
  char buffer[BUFFER_SIZE];
  format_duration(1001LL, buffer, BUFFER_SIZE);
  TEST_ASSERT_EQUAL_STRING("1 us", buffer);
}

void test_format_duration_millis(void) {
  char buffer[BUFFER_SIZE];
  format_duration(1000001LL, buffer, BUFFER_SIZE);
  TEST_ASSERT_EQUAL_STRING("1 ms", buffer);
}

void test_format_duration_seconds(void) {
  char buffer[BUFFER_SIZE];
  format_duration(1000000001LL, buffer, BUFFER_SIZE);
  TEST_ASSERT_EQUAL_STRING("0m 1s", buffer);
}

void test_format_duration_minutes_and_seconds(void) {
  char buffer[BUFFER_SIZE];
  format_duration(123000000000LL, buffer, BUFFER_SIZE);
  TEST_ASSERT_EQUAL_STRING("2m 3s", buffer);
}

void test_format_duration_large_value(void) {
  char buffer[BUFFER_SIZE];
  format_duration(987654321000LL, buffer, BUFFER_SIZE);
  TEST_ASSERT_EQUAL_STRING("16m 27s", buffer);
}

void test_format_memory_usage_bytes(void) {
  char buffer[BUFFER_SIZE];
  format_memory_usage(1024, buffer, BUFFER_SIZE);
  TEST_ASSERT_EQUAL_STRING("1 KB", buffer);
}

void test_format_memory_usage_kilobytes(void) {
  char buffer[BUFFER_SIZE];
  format_memory_usage(2048, buffer, BUFFER_SIZE);
  TEST_ASSERT_EQUAL_STRING("2 KB", buffer);
}

void test_format_memory_usage_megabytes(void) {
  char buffer[BUFFER_SIZE];
  format_memory_usage(1048576, buffer, BUFFER_SIZE);
  TEST_ASSERT_EQUAL_STRING("1.00 MB", buffer);
}

void test_format_memory_usage_gigabytes(void) {
  char buffer[BUFFER_SIZE];
  format_memory_usage(1073741824, buffer, BUFFER_SIZE);
  TEST_ASSERT_EQUAL_STRING("1.00 GB", buffer);
}

void test_format_memory_usage_terabytes(void) {
  char buffer[BUFFER_SIZE];
  format_memory_usage(1099511627776, buffer, BUFFER_SIZE);
  TEST_ASSERT_EQUAL_STRING("1024.00 GB", buffer);
}
