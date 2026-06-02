
#include "simple_bench.h"  // Include the header file where format_duration is declared
#include "unity.h"

#define BUFFER_SIZE 50

void setUp() {}

void tearDown() {}

static void noop_work(void) {
  volatile int value = 0;
  for (int i = 0; i < 1000; ++i) {
    value += i;
  }
}

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

void test_cputime_macro_returns_non_negative(void) {
  long long elapsed = CPUTIME(noop_work());
  TEST_ASSERT_TRUE(elapsed >= 0);
}

void test_rss_macro_returns_delta_value(void) {
  long delta = RSS(noop_work());
  TEST_ASSERT_TRUE(delta >= 0);
}

void test_timeval_diff_to_ns(void) {
  struct timeval start = {.tv_sec = 1, .tv_usec = 500000};
  struct timeval end = {.tv_sec = 2, .tv_usec = 250000};
  long long diff = timeval_diff_to_ns(&start, &end);
  TEST_ASSERT_EQUAL_INT64(750000000LL, diff);
}

void test_cputime_rss_usr_sys_macro_returns_non_negative(void) {
  long long wall = -1;
  long rss = -1;
  long long user = -1;
  long long system = -1;
  CPUTIME_RSS_USR_SYS(noop_work(), &wall, &rss, &user, &system);
  TEST_ASSERT_TRUE(wall >= 0);
  TEST_ASSERT_TRUE(rss >= 0);
  TEST_ASSERT_TRUE(user >= 0);
  TEST_ASSERT_TRUE(system >= 0);
}

void test_bench_csv_creates_header(void) {
  char filename[] = "/tmp/simple_bench_csv_XXXXXX";
  int fd = mkstemp(filename);
  TEST_ASSERT_TRUE(fd >= 0);
  close(fd);
  remove(filename);

  BENCH_CSV(noop_work(), filename, "unit-test");

  FILE* file = fopen(filename, "r");
  TEST_ASSERT_NOT_NULL(file);
  char line[256];
  char* first_line = fgets(line, sizeof(line), file);
  fclose(file);
  TEST_ASSERT_NOT_NULL(first_line);
  TEST_ASSERT_NOT_NULL(strstr(line, "function,wall_time_ns,user_time_ns"));

  remove(filename);
}
