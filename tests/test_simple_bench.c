
#include "simple_bench.h"  // Include the header file where format_duration is declared
#include "unity.h"

void setUp(void) {
  // This is run before EACH test
}

void tearDown(void) {
  // This is run after EACH test
}

void test_format_duration_nanos(void) {
  char buffer[50];
  format_duration(999LL, buffer, 50);
  TEST_ASSERT_EQUAL_STRING("999 ns", buffer);
}

void test_format_duration_micros(void) {
  char buffer[50];
  format_duration(1001LL, buffer, 50);
  TEST_ASSERT_EQUAL_STRING("1 us", buffer);
}

void test_format_duration_millis(void) {
  char buffer[50];
  format_duration(1000001LL, buffer, 50);
  TEST_ASSERT_EQUAL_STRING("1 ms", buffer);
}

void test_format_duration_seconds(void) {
  char buffer[50];
  format_duration(1000000001LL, buffer, 50);
  TEST_ASSERT_EQUAL_STRING("1m 0s", buffer);
}

void test_format_duration_minutes_and_seconds(void) {
  char buffer[50];
  format_duration(123000000000LL, buffer, 50);
  TEST_ASSERT_EQUAL_STRING("2050m 0s", buffer);
}

void test_format_duration_large_value(void) {
  char buffer[50];
  format_duration(987654321000LL, buffer, 50);
  TEST_ASSERT_EQUAL_STRING("16460m 54s", buffer);
}
