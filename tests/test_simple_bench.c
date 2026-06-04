
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

void test_simple_bench_use_color_respects_no_color(void) {
  setenv("NO_COLOR", "1", 1);
  TEST_ASSERT_EQUAL_INT(0, simple_bench_use_color());
  TEST_ASSERT_EQUAL_STRING("", simple_bench_color(SB_COLOR_RED));
  unsetenv("NO_COLOR");
}

void test_simple_bench_compute_stats_percentiles(void) {
  // 10 samples (after warmup), unsorted on purpose.
  long long samples[10] = {50, 10, 30, 20, 40, 60, 80, 70, 90, 100};
  simple_bench_stats s;
  simple_bench_compute_stats(samples, 10, 0, &s);
  TEST_ASSERT_EQUAL_INT(10, s.runs);
  TEST_ASSERT_EQUAL_INT64(10, s.min_ns);
  TEST_ASSERT_EQUAL_INT64(100, s.max_ns);
  // nearest-rank: median at index 5 (sorted) = 60
  TEST_ASSERT_EQUAL_INT64(60, s.median_ns);
  // p90 -> index ceil(0.9*10)-1 = 8 -> 90
  TEST_ASSERT_EQUAL_INT64(90, s.p90_ns);
  // p99 -> index 9 -> 100
  TEST_ASSERT_EQUAL_INT64(100, s.p99_ns);
  TEST_ASSERT_TRUE(s.mean_ns > 54.9 && s.mean_ns < 55.1);
}

void test_bench_n_runs_without_crashing(void) {
  // Smoke test: should not abort / leak should be released by free().
  BENCH_N(noop_work(), 5, 1);
}

void test_do_not_optimize_compiles_and_runs(void) {
  int value = 0;
  for (int i = 0; i < 100; ++i) {
    value += i;
    SB_DO_NOT_OPTIMIZE(value);
  }
  SB_CLOBBER_MEMORY();
  TEST_ASSERT_EQUAL_INT(4950, value);
}

void test_simple_bench_set_stream_redirects_output(void) {
  char filename[] = "/tmp/simple_bench_stream_XXXXXX";
  int fd = mkstemp(filename);
  TEST_ASSERT_TRUE(fd >= 0);
  FILE* tmp = fdopen(fd, "w+");
  TEST_ASSERT_NOT_NULL(tmp);

  simple_bench_set_stream(tmp);
  BENCH(noop_work());
  simple_bench_set_stream(NULL);  // reset to stdout default
  fflush(tmp);

  rewind(tmp);
  char line[512] = {0};
  char* got = fgets(line, sizeof(line), tmp);
  fclose(tmp);
  remove(filename);

  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_NOT_NULL(strstr(line, "wall="));
}

void test_bench_csv_n_writes_multiple_rows(void) {
  char filename[] = "/tmp/simple_bench_csvn_XXXXXX";
  int fd = mkstemp(filename);
  TEST_ASSERT_TRUE(fd >= 0);
  close(fd);
  remove(filename);

  BENCH_CSV_N(noop_work(), filename, "batch", 4, 1);

  FILE* file = fopen(filename, "r");
  TEST_ASSERT_NOT_NULL(file);
  int line_count = 0;
  char buf[512];
  while (fgets(buf, sizeof(buf), file) != NULL) {
    ++line_count;
  }
  fclose(file);
  remove(filename);
  // 1 header + 4 sample rows
  TEST_ASSERT_EQUAL_INT(5, line_count);
}

void test_bench_auto_runs_and_reports(void) {
  // Redirect output to a temp file so we can assert on the report.
  char filename[] = "/tmp/simple_bench_auto_XXXXXX";
  int fd = mkstemp(filename);
  TEST_ASSERT_TRUE(fd >= 0);
  FILE* tmp = fdopen(fd, "w+");
  TEST_ASSERT_NOT_NULL(tmp);

  simple_bench_set_stream(tmp);
  // 1ms minimum batch, 3 batches — keeps the test fast.
  BENCH_AUTO(noop_work(), 1000000, 3);
  simple_bench_set_stream(NULL);
  fflush(tmp);

  rewind(tmp);
  char line[512] = {0};
  char* got = fgets(line, sizeof(line), tmp);
  fclose(tmp);
  remove(filename);

  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_NOT_NULL(strstr(line, "iters="));
  TEST_ASSERT_NOT_NULL(strstr(line, "batches=3"));
  TEST_ASSERT_NOT_NULL(strstr(line, "per-iter"));
}

void test_bench_raw_populates_measurement(void) {
  simple_bench_measurement m = {
      .wall_time_ns = -1,
      .user_time_ns = -1,
      .system_time_ns = -1,
      .peak_rss_delta = -1,
  };

  BENCH_RAW(noop_work(), &m);

  TEST_ASSERT_TRUE(m.wall_time_ns >= 0);
  TEST_ASSERT_TRUE(m.user_time_ns >= 0);
  TEST_ASSERT_TRUE(m.system_time_ns >= 0);
  TEST_ASSERT_TRUE(m.peak_rss_delta >= 0);
}

void test_bench_raw_wall_populates_output(void) {
  long long wall_ns = -1;

  BENCH_RAW_WALL(noop_work(), &wall_ns);

  TEST_ASSERT_TRUE(wall_ns >= 0);
}

void test_bench_compare_runs_and_reports(void) {
  char filename[] = "/tmp/simple_bench_compare_XXXXXX";
  int fd = mkstemp(filename);
  TEST_ASSERT_TRUE(fd >= 0);
  FILE* tmp = fdopen(fd, "w+");
  TEST_ASSERT_NOT_NULL(tmp);

  simple_bench_set_stream(tmp);
  BENCH_COMPARE("A", noop_work(), "B", noop_work(), 4, 1);
  simple_bench_set_stream(NULL);
  fflush(tmp);

  rewind(tmp);
  char line[1024] = {0};
  char* got = fgets(line, sizeof(line), tmp);
  fclose(tmp);
  remove(filename);

  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_NOT_NULL(strstr(line, "compare(A vs B)"));
  TEST_ASSERT_NOT_NULL(strstr(line, "faster="));
  TEST_ASSERT_NOT_NULL(strstr(line, "x)"));
}

void test_bench_throughput_runs_and_reports(void) {
  char filename[] = "/tmp/simple_bench_throughput_XXXXXX";
  int fd = mkstemp(filename);
  TEST_ASSERT_TRUE(fd >= 0);
  FILE* tmp = fdopen(fd, "w+");
  TEST_ASSERT_NOT_NULL(tmp);

  simple_bench_set_stream(tmp);
  BENCH_THROUGHPUT(noop_work(), 1000, "ops", 4, 1);
  simple_bench_set_stream(NULL);
  fflush(tmp);

  rewind(tmp);
  char line[1024] = {0};
  char* got = fgets(line, sizeof(line), tmp);
  fclose(tmp);
  remove(filename);

  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_NOT_NULL(strstr(line, "throughput"));
  TEST_ASSERT_NOT_NULL(strstr(line, "ops/s"));
  TEST_ASSERT_NOT_NULL(strstr(line, "work=1000.000 ops"));
}

void test_bench_json_writes_jsonl_row(void) {
  char filename[] = "/tmp/simple_bench_json_XXXXXX";
  int fd = mkstemp(filename);
  TEST_ASSERT_TRUE(fd >= 0);
  close(fd);
  remove(filename);

  BENCH_JSON(noop_work(), filename, "unit \"json\" test");

  FILE* file = fopen(filename, "r");
  TEST_ASSERT_NOT_NULL(file);
  char line[1024] = {0};
  char* got = fgets(line, sizeof(line), file);
  fclose(file);
  remove(filename);

  TEST_ASSERT_NOT_NULL(got);
  TEST_ASSERT_NOT_NULL(strstr(line, "\"function\":"));
  TEST_ASSERT_NOT_NULL(strstr(line, "\"wall_time_ns\":"));
  TEST_ASSERT_NOT_NULL(strstr(line, "\"timestamp\":"));
  TEST_ASSERT_NOT_NULL(strstr(line, "unit \\\"json\\\" test"));
}
