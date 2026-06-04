#ifndef SIMPLE_BENCH_H
#define SIMPLE_BENCH_H

// simple_bench — header-only micro-benchmark helpers.
//
// Threading notes:
//   - All measurement macros use RUSAGE_SELF, which aggregates user/system
//     CPU time across all threads of the process. When benchmarking a
//     multi-threaded FCALL, user_time_ns/system_time_ns therefore represent
//     the *sum* over threads, not per-thread cost.
//   - print_*/write_* helpers are NOT thread-safe; only call from a single
//     thread at a time, or guard with your own mutex.
//   - simple_bench_set_stream() / simple_bench_use_color() use process-wide
//     state.
//
// Memory notes:
//   - peak_rss_delta is derived from getrusage(ru_maxrss), which is a
//     high-water mark. The value reflects the *growth* in process peak RSS
//     during the call; it is an approximation, not exact per-call allocation
//     accounting.

#ifdef __cplusplus
extern "C" {
#endif

#include <errno.h>
#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

// Raw ANSI color escape sequences. Use simple_bench_color() to obtain the
// proper code (or an empty string when colors are disabled).
#define SB_ANSI_BLUE  "\x1B[34m"
#define SB_ANSI_RED   "\x1B[31m"
#define SB_ANSI_WHITE "\x1B[37m"
#define SB_ANSI_RESET "\x1B[0m"

// Backwards compatible aliases (always raw codes; older code may depend on
// them being string literals).
#define BLUE  SB_ANSI_BLUE
#define RED   SB_ANSI_RED
#define WHITE SB_ANSI_WHITE
#define RESET SB_ANSI_RESET

typedef enum simple_bench_color_id {
  SB_COLOR_RESET = 0,
  SB_COLOR_BLUE,
  SB_COLOR_RED,
  SB_COLOR_WHITE
} simple_bench_color_id;

// Pluggable output stream for human-readable BENCH/BENCH_VERBOSE/BENCH_N
// output. Defaults to stdout when NULL. Use simple_bench_set_stream(stderr)
// to keep program output separate from benchmark diagnostics.
static inline FILE** simple_bench_stream_slot(void) {
  static FILE* sb_stream = NULL;
  return &sb_stream;
}

static inline FILE* simple_bench_stream(void) {
  FILE* s = *simple_bench_stream_slot();
  return s != NULL ? s : stdout;
}

static inline void simple_bench_set_stream(FILE* stream) {
  *simple_bench_stream_slot() = stream;
}

// Returns 1 if ANSI colors should be emitted on the active output stream,
// 0 otherwise. Honors the NO_COLOR convention (https://no-color.org) and
// falls back to isatty() so redirected output stays clean.
static inline int simple_bench_use_color(void) {
  const char* no_color = getenv("NO_COLOR");
  if (no_color != NULL && no_color[0] != '\0') {
    return 0;
  }
  FILE* s = simple_bench_stream();
  int fd = fileno(s);
  return (fd >= 0 && isatty(fd)) ? 1 : 0;
}

static inline const char* simple_bench_color(simple_bench_color_id id) {
  if (!simple_bench_use_color()) {
    return "";
  }
  switch (id) {
    case SB_COLOR_BLUE:  return SB_ANSI_BLUE;
    case SB_COLOR_RED:   return SB_ANSI_RED;
    case SB_COLOR_WHITE: return SB_ANSI_WHITE;
    case SB_COLOR_RESET:
    default:             return SB_ANSI_RESET;
  }
}

// Optimizer barrier: prevents the compiler from eliding a value that is the
// result of work we want to measure. Use as
//   int result = compute();
//   SB_DO_NOT_OPTIMIZE(result);
// inside the FCALL block when benchmarking small pure functions.
#if defined(__GNUC__) || defined(__clang__)
#define SB_DO_NOT_OPTIMIZE(x) \
  __asm__ __volatile__("" : : "g"(x) : "memory")
#define SB_CLOBBER_MEMORY() \
  __asm__ __volatile__("" : : : "memory")
#else
#define SB_DO_NOT_OPTIMIZE(x) ((void)(x))
#define SB_CLOBBER_MEMORY()   ((void)0)
#endif

typedef struct simple_bench_measurement {
  long long wall_time_ns;
  long long user_time_ns;
  long long system_time_ns;
  // Difference between getrusage(ru_maxrss) before and after the call.
  // NOTE: ru_maxrss is a high-water mark, so this is the *growth* of the
  // process peak RSS caused by the call, not its per-call allocation. The
  // value can be 0 (no new peak) or negative on some platforms due to OS
  // accounting; treat it as an indicator, not a precise measurement.
  long peak_rss_delta;
} simple_bench_measurement;

typedef struct simple_bench_stats {
  int runs;
  int warmup;
  long long min_ns;
  long long max_ns;
  long long median_ns;
  long long p90_ns;
  long long p99_ns;
  double mean_ns;
} simple_bench_stats;

static inline int simple_bench_cmp_ll(const void* a, const void* b) {
  long long lhs = *(const long long*)a;
  long long rhs = *(const long long*)b;
  return (lhs > rhs) - (lhs < rhs);
}

// Compute stats from an unsorted samples buffer of length n (n > 0).
// The buffer is sorted in-place.
static inline void simple_bench_compute_stats(long long* samples, int n,
                                              int warmup,
                                              simple_bench_stats* out) {
  qsort(samples, (size_t)n, sizeof(long long), simple_bench_cmp_ll);
  out->runs = n;
  out->warmup = warmup;
  out->min_ns = samples[0];
  out->max_ns = samples[n - 1];
  out->median_ns = samples[n / 2];
  // Nearest-rank percentile: index = ceil(p/100 * n) - 1, clamped.
  int idx90 = (90 * n + 99) / 100 - 1;
  int idx99 = (99 * n + 99) / 100 - 1;
  if (idx90 < 0) idx90 = 0;
  if (idx99 < 0) idx99 = 0;
  if (idx90 >= n) idx90 = n - 1;
  if (idx99 >= n) idx99 = n - 1;
  out->p90_ns = samples[idx90];
  out->p99_ns = samples[idx99];
  double sum = 0.0;
  for (int i = 0; i < n; ++i) {
    sum += (double)samples[i];
  }
  out->mean_ns = sum / (double)n;
}

// Human readable time format
static inline void format_duration(long long nanoseconds, char* buffer,
                                   size_t buffer_size) {
  const long long nanos_per_second = 1000000000LL;
  const long long nanos_per_millisecond = 1000000LL;
  const long long nanos_per_microsecond = 1000LL;
  const long long seconds_per_minute = 60LL;

  if (nanoseconds < nanos_per_microsecond) {
    snprintf(buffer, buffer_size, "%lld ns", nanoseconds);
  } else if (nanoseconds < nanos_per_millisecond) {
    snprintf(buffer, buffer_size, "%lld us",
             nanoseconds / nanos_per_microsecond);
  } else if (nanoseconds < nanos_per_second) {
    snprintf(buffer, buffer_size, "%lld ms",
             nanoseconds / nanos_per_millisecond);
  } else {
    long long minutes = nanoseconds / (nanos_per_second * seconds_per_minute);
    long long seconds =
        (nanoseconds % (nanos_per_second * seconds_per_minute)) /
        nanos_per_second;
    snprintf(buffer, buffer_size, "%lldm %llds", minutes, seconds);
  }
}

// Human readable memory format
static inline void format_memory_usage(long memory_usage, char* buffer,
                                       size_t buffer_size) {
  const long kb = 1024;
  const long mb = 1024 * kb;

#ifdef __linux__
  // On Linux, ru_maxrss is in kilobytes
  long memory_kilobytes = memory_usage;
#else
  // On BSD and macOS, ru_maxrss is in bytes
  long memory_kilobytes = memory_usage / kb;
#endif

  if (memory_kilobytes < kb) {
    snprintf(buffer, buffer_size, "%ld KB", memory_kilobytes);
  } else if (memory_kilobytes < mb) {
    snprintf(buffer, buffer_size, "%.2f MB", memory_kilobytes / (double)kb);
  } else {
    snprintf(buffer, buffer_size, "%.2f GB", memory_kilobytes / (double)mb);
  }
}

static inline long long timeval_diff_to_ns(const struct timeval* start,
                                           const struct timeval* end) {
  long long sec = (long long)(end->tv_sec - start->tv_sec);
  long long usec = (long long)(end->tv_usec - start->tv_usec);
  return sec * 1000000000LL + usec * 1000LL;
}

static inline long get_memory_usage(void) {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return usage.ru_maxrss;
}

static inline void print_benchmark_result_brief(
    const simple_bench_measurement* measurement, const char* fcall) {
  char formatted_wall[64];
  char formatted_memory[64];
  format_duration(measurement->wall_time_ns, formatted_wall,
                  sizeof(formatted_wall));
  format_memory_usage(measurement->peak_rss_delta, formatted_memory,
                      sizeof(formatted_memory));
  fprintf(simple_bench_stream(),
          "B| %s | wall=%s | peak-rss-delta=%s\n", fcall, formatted_wall,
          formatted_memory);
}

static inline void print_benchmark_result_verbose(
    const simple_bench_measurement* measurement, const char* fcall,
    const char* file, int line) {
  char formatted_wall[64];
  char formatted_user[64];
  char formatted_system[64];
  char formatted_memory[64];
  format_duration(measurement->wall_time_ns, formatted_wall,
                  sizeof(formatted_wall));
  format_duration(measurement->user_time_ns, formatted_user,
                  sizeof(formatted_user));
  format_duration(measurement->system_time_ns, formatted_system,
                  sizeof(formatted_system));
  format_memory_usage(measurement->peak_rss_delta, formatted_memory,
                      sizeof(formatted_memory));
  const char* c_blue = simple_bench_color(SB_COLOR_BLUE);
  const char* c_red = simple_bench_color(SB_COLOR_RED);
  const char* c_white = simple_bench_color(SB_COLOR_WHITE);
  const char* c_reset = simple_bench_color(SB_COLOR_RESET);
  const char* rss_hint =
      measurement->peak_rss_delta <= 0 ? " (no new peak)" : "";
  fprintf(simple_bench_stream(),
          "%sℬ|%s Wall-Time: %s%s\t%sUser: %s%s\t%sSystem: %s%s\t%s"
          "Peak-RSS-Delta: %s%s%s\t%s| %s in %s%s:%i%s\n",
          c_blue, c_reset, c_red, formatted_wall, c_reset, c_red,
          formatted_user, c_reset, c_red, formatted_system, c_reset, c_red,
          formatted_memory, rss_hint, c_reset, fcall, c_white, file, line,
          c_reset);
}

static inline void print_benchmark_stats_summary(const char* fcall,
                                                 const simple_bench_stats* s) {
  char min_buf[64], median_buf[64], mean_buf[64], p90_buf[64], p99_buf[64],
      max_buf[64];
  format_duration(s->min_ns, min_buf, sizeof(min_buf));
  format_duration(s->median_ns, median_buf, sizeof(median_buf));
  format_duration((long long)s->mean_ns, mean_buf, sizeof(mean_buf));
  format_duration(s->p90_ns, p90_buf, sizeof(p90_buf));
  format_duration(s->p99_ns, p99_buf, sizeof(p99_buf));
  format_duration(s->max_ns, max_buf, sizeof(max_buf));
  fprintf(simple_bench_stream(),
          "B| %s | n=%d warmup=%d | wall min/median/mean/p90/p99/max = "
          "%s / %s / %s / %s / %s / %s\n",
          fcall, s->runs, s->warmup, min_buf, median_buf, mean_buf, p90_buf,
          p99_buf, max_buf);
}

static inline void print_benchmark_auto_summary(const char* fcall,
                                                long long iters_per_batch,
                                                int batches,
                                                const simple_bench_stats* s) {
  char min_buf[64], median_buf[64], mean_buf[64], p90_buf[64], p99_buf[64],
      max_buf[64];
  format_duration(s->min_ns, min_buf, sizeof(min_buf));
  format_duration(s->median_ns, median_buf, sizeof(median_buf));
  format_duration((long long)s->mean_ns, mean_buf, sizeof(mean_buf));
  format_duration(s->p90_ns, p90_buf, sizeof(p90_buf));
  format_duration(s->p99_ns, p99_buf, sizeof(p99_buf));
  format_duration(s->max_ns, max_buf, sizeof(max_buf));
  fprintf(simple_bench_stream(),
          "B| %s | iters=%lld batches=%d | per-iter "
          "min/median/mean/p90/p99/max = %s / %s / %s / %s / %s / %s\n",
          fcall, iters_per_batch, batches, min_buf, median_buf, mean_buf,
          p90_buf, p99_buf, max_buf);
}

static inline void print_benchmark_compare_summary(
    const char* name_a, const simple_bench_stats* stats_a, const char* name_b,
    const simple_bench_stats* stats_b, int warmup) {
  char a_med[64], b_med[64], a_mean[64], b_mean[64];
  format_duration(stats_a->median_ns, a_med, sizeof(a_med));
  format_duration(stats_b->median_ns, b_med, sizeof(b_med));
  format_duration((long long)stats_a->mean_ns, a_mean, sizeof(a_mean));
  format_duration((long long)stats_b->mean_ns, b_mean, sizeof(b_mean));

  double med_a = (double)stats_a->median_ns;
  double med_b = (double)stats_b->median_ns;
  const char* faster = med_a <= med_b ? name_a : name_b;
  double faster_ns = med_a <= med_b ? med_a : med_b;
  double slower_ns = med_a <= med_b ? med_b : med_a;
  double speedup = (faster_ns > 0.0) ? (slower_ns / faster_ns) : 0.0;

  fprintf(simple_bench_stream(),
          "B| compare(%s vs %s) | n=%d warmup=%d | median: %s=%s, %s=%s | "
          "mean: %s=%s, %s=%s | faster=%s (%.2fx)\n",
          name_a, name_b, stats_a->runs, warmup, name_a, a_med, name_b, b_med,
          name_a, a_mean, name_b, b_mean, faster, speedup);
}

static inline void print_benchmark_throughput_summary(
    const char* fcall, const simple_bench_stats* stats, double work_units,
    const char* unit_label) {
  char med_buf[64], mean_buf[64];
  format_duration(stats->median_ns, med_buf, sizeof(med_buf));
  format_duration((long long)stats->mean_ns, mean_buf, sizeof(mean_buf));

  const char* label = (unit_label != NULL && unit_label[0] != '\0')
                          ? unit_label
                          : "units";

  double median_ns = (double)stats->median_ns;
  double mean_ns = stats->mean_ns;
  double median_per_sec = median_ns > 0.0 ? (work_units * 1e9) / median_ns : 0.0;
  double mean_per_sec = mean_ns > 0.0 ? (work_units * 1e9) / mean_ns : 0.0;

  fprintf(simple_bench_stream(),
          "B| %s | n=%d warmup=%d | wall median=%s mean=%s | work=%.3f %s | "
          "throughput median=%.3f %s/s mean=%.3f %s/s\n",
          fcall, stats->runs, stats->warmup, med_buf, mean_buf, work_units,
          label, median_per_sec, label, mean_per_sec, label);
}

static inline void write_csv_escaped(FILE* file, const char* value) {
  const char* cursor = value == NULL ? "" : value;
  fputc('"', file);
  while (*cursor != '\0') {
    if (*cursor == '"') {
      fputc('"', file);
    }
    fputc(*cursor, file);
    ++cursor;
  }
  fputc('"', file);
}

static inline void write_json_escaped(FILE* file, const char* value) {
  const unsigned char* cursor =
      (const unsigned char*)(value == NULL ? "" : value);
  fputc('"', file);
  while (*cursor != '\0') {
    unsigned char c = *cursor;
    switch (c) {
      case '"':
        fputs("\\\"", file);
        break;
      case '\\':
        fputs("\\\\", file);
        break;
      case '\b':
        fputs("\\b", file);
        break;
      case '\f':
        fputs("\\f", file);
        break;
      case '\n':
        fputs("\\n", file);
        break;
      case '\r':
        fputs("\\r", file);
        break;
      case '\t':
        fputs("\\t", file);
        break;
      default:
        if (c < 0x20) {
          fprintf(file, "\\u%04x", (unsigned int)c);
        } else {
          fputc((int)c, file);
        }
        break;
    }
    ++cursor;
  }
  fputc('"', file);
}

// Writes the CSV header line if needed. file must be opened with mode "a+"
// or similar and seekable. Returns 0 on success, -1 on failure.
static inline int simple_bench_csv_write_header_if_empty(FILE* file) {
  if (fseek(file, 0, SEEK_END) != 0) {
    fprintf(stderr, "simple_bench: fseek failed: %s\n", strerror(errno));
    return -1;
  }
  long file_size = ftell(file);
  if (file_size < 0) {
    fprintf(stderr, "simple_bench: ftell failed: %s\n", strerror(errno));
    return -1;
  }
  if (file_size == 0) {
    fprintf(file,
            "function,wall_time_ns,user_time_ns,system_time_ns,peak_rss_delta,"
            "file,line,timestamp,comment\n");
  }
  return 0;
}

// Appends a single CSV row to an already-opened file handle. Cheap enough to
// be called inside hot loops because it does no fopen/fseek work.
static inline void simple_bench_csv_append_row(FILE* file,
                                               const char* function_name,
                                               const char* file_name, int line,
                                               const char* comment,
                                               const simple_bench_measurement* m) {
  time_t now = time(NULL);
  struct tm local_time_value;
  struct tm* local_time = localtime_r(&now, &local_time_value);
  char timestamp[32];
  if (local_time == NULL ||
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S",
               local_time) == 0) {
    snprintf(timestamp, sizeof(timestamp), "unknown");
  }

  write_csv_escaped(file, function_name);
  fprintf(file, ",%lld,%lld,%lld,%ld,", m->wall_time_ns, m->user_time_ns,
          m->system_time_ns, m->peak_rss_delta);
  write_csv_escaped(file, file_name);
  fprintf(file, ",%d,", line);
  write_csv_escaped(file, timestamp);
  fprintf(file, ",");
  write_csv_escaped(file, comment);
  fprintf(file, "\n");
}

// Returns 0 on success, -1 on failure. Failures are reported to stderr but
// never abort the program (header-only libraries should not call exit()).
// NOTE: opens, seeks and closes the file on every call. For loops, prefer
// BENCH_CSV_N or the simple_bench_csv_* helpers with a single open handle.
static inline int write_benchmark_csv_row(const char* filename,
                                          const char* function_name,
                                          const char* file_name, int line,
                                          const char* comment,
                                          const simple_bench_measurement* m) {
  FILE* file = fopen(filename, "a+");
  if (file == NULL) {
    fprintf(stderr, "simple_bench: fopen(%s) failed: %s\n", filename,
            strerror(errno));
    return -1;
  }
  if (simple_bench_csv_write_header_if_empty(file) != 0) {
    fclose(file);
    return -1;
  }
  simple_bench_csv_append_row(file, function_name, file_name, line, comment, m);
  fclose(file);
  return 0;
}

// Appends one JSON object per line (JSONL/NDJSON).
static inline int write_benchmark_json_row(const char* filename,
                                           const char* function_name,
                                           const char* file_name, int line,
                                           const char* comment,
                                           const simple_bench_measurement* m) {
  FILE* file = fopen(filename, "a");
  if (file == NULL) {
    fprintf(stderr, "simple_bench: fopen(%s) failed: %s\n", filename,
            strerror(errno));
    return -1;
  }

  time_t now = time(NULL);
  struct tm local_time_value;
  struct tm* local_time = localtime_r(&now, &local_time_value);
  char timestamp[32];
  if (local_time == NULL ||
      strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S",
               local_time) == 0) {
    snprintf(timestamp, sizeof(timestamp), "unknown");
  }

  fputc('{', file);
  fputs("\"function\":", file);
  write_json_escaped(file, function_name);
  fprintf(file,
          ",\"wall_time_ns\":%lld,\"user_time_ns\":%lld,\"system_time_ns\":"
          "%lld,\"peak_rss_delta\":%ld,",
          m->wall_time_ns, m->user_time_ns, m->system_time_ns,
          m->peak_rss_delta);
  fputs("\"file\":", file);
  write_json_escaped(file, file_name);
  fprintf(file, ",\"line\":%d,", line);
  fputs("\"timestamp\":", file);
  write_json_escaped(file, timestamp);
  fputs(",\"comment\":", file);
  write_json_escaped(file, comment);
  fputs("}\n", file);

  fclose(file);
  return 0;
}

static inline void print_environment_info(void) {
  FILE* out = simple_bench_stream();
  const char* c_blue = simple_bench_color(SB_COLOR_BLUE);
  const char* c_reset = simple_bench_color(SB_COLOR_RESET);
  struct utsname sysinfo;
  if (uname(&sysinfo) == 0) {
    fprintf(out, "\r%sℬ|%s OS: %s %s (%s)", c_blue, c_reset, sysinfo.sysname,
            sysinfo.release, sysinfo.machine);
  }

  long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
  if (nprocs > 0) {
    fprintf(out, " | Cores: %ld", nprocs);
  }

  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  if (pages > 0 && page_size > 0) {
    double mem_gb = (pages * page_size) / (1024.0 * 1024.0 * 1024.0);
    fprintf(out, " | RAM: %.2f GB", mem_gb);
  }

  double loadavg[3];
  if (getloadavg(loadavg, 3) == 3) {
    fprintf(out, " | Load: %.2f / %.2f / %.2f", loadavg[0], loadavg[1],
            loadavg[2]);
  }

  fputc('\n', out);
  fflush(out);
}

// CPUTIME macro (returns wall-clock elapsed nanoseconds for FCALL).
#define CPUTIME(FCALL)                                                       \
  __extension__({                                                            \
    struct timespec sb_cputime_start, sb_cputime_end;                        \
    long long sb_cputime_elapsed;                                            \
    if (clock_gettime(CLOCK_MONOTONIC, &sb_cputime_start) != 0) {            \
      perror("clock_gettime");                                               \
      sb_cputime_elapsed = -1;                                               \
    } else {                                                                 \
      FCALL;                                                                 \
      if (clock_gettime(CLOCK_MONOTONIC, &sb_cputime_end) != 0) {            \
        perror("clock_gettime");                                             \
        sb_cputime_elapsed = -1;                                             \
      } else {                                                               \
        sb_cputime_elapsed =                                                 \
            (sb_cputime_end.tv_sec - sb_cputime_start.tv_sec) * 1000000000LL + \
            (sb_cputime_end.tv_nsec - sb_cputime_start.tv_nsec);             \
      }                                                                      \
    }                                                                        \
    sb_cputime_elapsed;                                                      \
  })

#define CPUTIME_RSS(FCALL, TIME_PTR, RSS_PTR)                                \
  do {                                                                       \
    simple_bench_measurement sb_result;                                      \
    CPUTIME_RSS_USR_SYS(FCALL, &sb_result.wall_time_ns, &sb_result.peak_rss_delta, \
                        &sb_result.user_time_ns, &sb_result.system_time_ns); \
    *(TIME_PTR) = sb_result.wall_time_ns;                                    \
    *(RSS_PTR) = sb_result.peak_rss_delta;                                   \
  } while (0)

#define CPUTIME_RSS_USR_SYS(FCALL, WALL_PTR, RSS_PTR, USER_PTR, SYSTEM_PTR)  \
  do {                                                                       \
    struct timespec sb_start, sb_end;                                        \
    struct rusage sb_usage_before, sb_usage_after;                           \
    long sb_memory_before, sb_memory_after;                                  \
    sb_memory_before = get_memory_usage();                                   \
    if (getrusage(RUSAGE_SELF, &sb_usage_before) != 0) {                     \
      perror("getrusage");                                                   \
      *(WALL_PTR) = -1;                                                      \
      *(USER_PTR) = -1;                                                      \
      *(SYSTEM_PTR) = -1;                                                    \
      *(RSS_PTR) = -1;                                                       \
    } else if (clock_gettime(CLOCK_MONOTONIC, &sb_start) != 0) {             \
      perror("clock_gettime");                                               \
      *(WALL_PTR) = -1;                                                      \
      *(USER_PTR) = -1;                                                      \
      *(SYSTEM_PTR) = -1;                                                    \
      *(RSS_PTR) = -1;                                                       \
    } else {                                                                 \
      FCALL;                                                                 \
      if (clock_gettime(CLOCK_MONOTONIC, &sb_end) != 0) {                    \
        perror("clock_gettime");                                             \
        *(WALL_PTR) = -1;                                                    \
      } else {                                                               \
        *(WALL_PTR) = (sb_end.tv_sec - sb_start.tv_sec) * 1000000000LL +     \
                      (sb_end.tv_nsec - sb_start.tv_nsec);                   \
      }                                                                      \
      if (getrusage(RUSAGE_SELF, &sb_usage_after) != 0) {                    \
        perror("getrusage");                                                 \
        *(USER_PTR) = -1;                                                    \
        *(SYSTEM_PTR) = -1;                                                  \
      } else {                                                               \
        *(USER_PTR) = timeval_diff_to_ns(&sb_usage_before.ru_utime,           \
                                         &sb_usage_after.ru_utime);          \
        *(SYSTEM_PTR) = timeval_diff_to_ns(&sb_usage_before.ru_stime,         \
                                           &sb_usage_after.ru_stime);        \
      }                                                                      \
      sb_memory_after = get_memory_usage();                                  \
      *(RSS_PTR) = sb_memory_after - sb_memory_before;                       \
    }                                                                        \
  } while (0)

#define RSS(FCALL)                                                            \
  __extension__({                                                            \
    long sb_rss_before = get_memory_usage();                                 \
    FCALL;                                                                   \
    long sb_rss_after = get_memory_usage();                                  \
    long sb_rss_used = sb_rss_after - sb_rss_before;                         \
    sb_rss_used;                                                             \
  })

#define BENCH(FCALL)                                                          \
  do {                                                                       \
    simple_bench_measurement sb_result;                                      \
    CPUTIME_RSS_USR_SYS(FCALL, &sb_result.wall_time_ns, &sb_result.peak_rss_delta, \
                        &sb_result.user_time_ns, &sb_result.system_time_ns); \
    print_benchmark_result_brief(&sb_result, #FCALL);                        \
  } while (0)

#define BENCH_VERBOSE(FCALL)                                                  \
  do {                                                                       \
    simple_bench_measurement sb_result;                                      \
    CPUTIME_RSS_USR_SYS(FCALL, &sb_result.wall_time_ns, &sb_result.peak_rss_delta, \
                        &sb_result.user_time_ns, &sb_result.system_time_ns); \
    print_benchmark_result_verbose(&sb_result, #FCALL, __FILE__, __LINE__);  \
  } while (0)

// Raw measurement variant: performs one full measurement but does not print.
// OUT_PTR must point to a valid simple_bench_measurement.
#define BENCH_RAW(FCALL, OUT_PTR)                                             \
  do {                                                                       \
    simple_bench_measurement* sb_out = (OUT_PTR);                            \
    if (sb_out == NULL) {                                                    \
      fprintf(stderr, "BENCH_RAW: OUT_PTR must not be NULL\n");             \
      break;                                                                 \
    }                                                                        \
    CPUTIME_RSS_USR_SYS(FCALL, &sb_out->wall_time_ns, &sb_out->peak_rss_delta, \
                        &sb_out->user_time_ns, &sb_out->system_time_ns);     \
  } while (0)

// Wall-time-only raw measurement variant. OUT_NS_PTR must point to a valid
// long long destination.
#define BENCH_RAW_WALL(FCALL, OUT_NS_PTR)                                     \
  do {                                                                       \
    long long* sb_out_ns = (OUT_NS_PTR);                                     \
    if (sb_out_ns == NULL) {                                                 \
      fprintf(stderr, "BENCH_RAW_WALL: OUT_NS_PTR must not be NULL\n");     \
      break;                                                                 \
    }                                                                        \
    *sb_out_ns = CPUTIME(FCALL);                                             \
  } while (0)

// Compare two call sites under identical loop conditions.
// Reports wall-time stats (median/mean) for both and a median-based speedup.
#define BENCH_COMPARE(NAME_A, FCALL_A, NAME_B, FCALL_B, RUNS, WARMUP)       \
  do {                                                                       \
    int sb_runs = (RUNS);                                                    \
    int sb_warmup = (WARMUP);                                                \
    if (sb_runs <= 0) {                                                      \
      fprintf(stderr, "BENCH_COMPARE: RUNS must be > 0\n");                 \
      break;                                                                 \
    }                                                                        \
    if (sb_warmup < 0) {                                                     \
      fprintf(stderr, "BENCH_COMPARE: WARMUP must be >= 0\n");              \
      break;                                                                 \
    }                                                                        \
    for (int sb_i = 0; sb_i < sb_warmup; ++sb_i) {                           \
      FCALL_A;                                                               \
      FCALL_B;                                                               \
    }                                                                        \
    long long* sb_samples_a =                                                \
        (long long*)malloc((size_t)sb_runs * sizeof(long long));             \
    long long* sb_samples_b =                                                \
        (long long*)malloc((size_t)sb_runs * sizeof(long long));             \
    if (sb_samples_a == NULL || sb_samples_b == NULL) {                      \
      fprintf(stderr, "BENCH_COMPARE: malloc failed for %d samples\n",      \
              sb_runs);                                                      \
      free(sb_samples_a);                                                    \
      free(sb_samples_b);                                                    \
      break;                                                                 \
    }                                                                        \
    for (int sb_i = 0; sb_i < sb_runs; ++sb_i) {                             \
      simple_bench_measurement sb_ma;                                        \
      simple_bench_measurement sb_mb;                                        \
      if ((sb_i & 1) == 0) {                                                 \
        BENCH_RAW(FCALL_A, &sb_ma);                                          \
        BENCH_RAW(FCALL_B, &sb_mb);                                          \
      } else {                                                               \
        BENCH_RAW(FCALL_B, &sb_mb);                                          \
        BENCH_RAW(FCALL_A, &sb_ma);                                          \
      }                                                                      \
      sb_samples_a[sb_i] = sb_ma.wall_time_ns;                               \
      sb_samples_b[sb_i] = sb_mb.wall_time_ns;                               \
    }                                                                        \
    simple_bench_stats sb_stats_a;                                           \
    simple_bench_stats sb_stats_b;                                           \
    simple_bench_compute_stats(sb_samples_a, sb_runs, sb_warmup,             \
                               &sb_stats_a);                                 \
    simple_bench_compute_stats(sb_samples_b, sb_runs, sb_warmup,             \
                               &sb_stats_b);                                 \
    print_benchmark_compare_summary((NAME_A), &sb_stats_a, (NAME_B),         \
                                    &sb_stats_b, sb_warmup);                 \
    free(sb_samples_a);                                                      \
    free(sb_samples_b);                                                      \
  } while (0)

// Throughput benchmark for a call that processes a known amount of work.
// WORK_UNITS can be bytes, elements, requests, etc., described by UNIT_LABEL.
#define BENCH_THROUGHPUT(FCALL, WORK_UNITS, UNIT_LABEL, RUNS, WARMUP)       \
  do {                                                                       \
    int sb_runs = (RUNS);                                                    \
    int sb_warmup = (WARMUP);                                                \
    double sb_work_units = (double)(WORK_UNITS);                             \
    if (sb_runs <= 0) {                                                      \
      fprintf(stderr, "BENCH_THROUGHPUT: RUNS must be > 0\n");              \
      break;                                                                 \
    }                                                                        \
    if (sb_warmup < 0) {                                                     \
      fprintf(stderr, "BENCH_THROUGHPUT: WARMUP must be >= 0\n");           \
      break;                                                                 \
    }                                                                        \
    if (sb_work_units <= 0.0) {                                              \
      fprintf(stderr, "BENCH_THROUGHPUT: WORK_UNITS must be > 0\n");        \
      break;                                                                 \
    }                                                                        \
    for (int sb_i = 0; sb_i < sb_warmup; ++sb_i) {                           \
      FCALL;                                                                 \
    }                                                                        \
    long long* sb_samples =                                                  \
        (long long*)malloc((size_t)sb_runs * sizeof(long long));             \
    if (sb_samples == NULL) {                                                \
      fprintf(stderr, "BENCH_THROUGHPUT: malloc failed for %d samples\n",   \
              sb_runs);                                                      \
      break;                                                                 \
    }                                                                        \
    for (int sb_i = 0; sb_i < sb_runs; ++sb_i) {                             \
      long long sb_wall_ns = -1;                                             \
      BENCH_RAW_WALL(FCALL, &sb_wall_ns);                                    \
      sb_samples[sb_i] = sb_wall_ns;                                         \
    }                                                                        \
    simple_bench_stats sb_stats;                                             \
    simple_bench_compute_stats(sb_samples, sb_runs, sb_warmup, &sb_stats);   \
    print_benchmark_throughput_summary(#FCALL, &sb_stats, sb_work_units,     \
                                       (UNIT_LABEL));                        \
    free(sb_samples);                                                        \
  } while (0)

#define BENCH_N(FCALL, RUNS, WARMUP)                                         \
  do {                                                                       \
    int sb_runs = (RUNS);                                                    \
    int sb_warmup = (WARMUP);                                                \
    if (sb_runs <= 0) {                                                      \
      fprintf(stderr, "BENCH_N: RUNS must be > 0\n");                        \
      break;                                                                 \
    }                                                                        \
    if (sb_warmup < 0) {                                                     \
      fprintf(stderr, "BENCH_N: WARMUP must be >= 0\n");                     \
      break;                                                                 \
    }                                                                        \
    for (int sb_i = 0; sb_i < sb_warmup; ++sb_i) {                           \
      FCALL;                                                                 \
    }                                                                        \
    long long* sb_samples =                                                  \
        (long long*)malloc((size_t)sb_runs * sizeof(long long));             \
    if (sb_samples == NULL) {                                                \
      fprintf(stderr, "BENCH_N: malloc failed for %d samples\n", sb_runs);   \
      break;                                                                 \
    }                                                                        \
    for (int sb_i = 0; sb_i < sb_runs; ++sb_i) {                             \
      simple_bench_measurement sb_result;                                    \
      CPUTIME_RSS_USR_SYS(FCALL, &sb_result.wall_time_ns,                    \
                          &sb_result.peak_rss_delta,                         \
                          &sb_result.user_time_ns,                           \
                          &sb_result.system_time_ns);                        \
      sb_samples[sb_i] = sb_result.wall_time_ns;                             \
    }                                                                        \
    simple_bench_stats sb_stats;                                             \
    simple_bench_compute_stats(sb_samples, sb_runs, sb_warmup, &sb_stats);   \
    print_benchmark_stats_summary(#FCALL, &sb_stats);                        \
    free(sb_samples);                                                        \
  } while (0)

#define BENCH_CSV(FCALL, FILE_NAME, COMMENT)                                  \
  do {                                                                       \
    simple_bench_measurement sb_result;                                      \
    CPUTIME_RSS_USR_SYS(FCALL, &sb_result.wall_time_ns, &sb_result.peak_rss_delta, \
                        &sb_result.user_time_ns, &sb_result.system_time_ns); \
    write_benchmark_csv_row((FILE_NAME), #FCALL, __FILE__, __LINE__,         \
                            (COMMENT), &sb_result);                          \
  } while (0)

// Machine-readable JSONL output (one JSON object per line).
#define BENCH_JSON(FCALL, FILE_NAME, COMMENT)                                 \
  do {                                                                       \
    simple_bench_measurement sb_result;                                      \
    CPUTIME_RSS_USR_SYS(FCALL, &sb_result.wall_time_ns, &sb_result.peak_rss_delta, \
                        &sb_result.user_time_ns, &sb_result.system_time_ns); \
    write_benchmark_json_row((FILE_NAME), #FCALL, __FILE__, __LINE__,        \
                             (COMMENT), &sb_result);                         \
  } while (0)

// Batched CSV variant: opens FILE_NAME exactly once, runs FCALL RUNS times
// (plus WARMUP discarded warmup iterations), appends one row per run and
// closes the file. Much cheaper than calling BENCH_CSV in a loop because it
// avoids fopen/fseek/fclose overhead between samples.
#define BENCH_CSV_N(FCALL, FILE_NAME, COMMENT, RUNS, WARMUP)                 \
  do {                                                                       \
    int sb_runs = (RUNS);                                                    \
    int sb_warmup = (WARMUP);                                                \
    if (sb_runs <= 0) {                                                      \
      fprintf(stderr, "BENCH_CSV_N: RUNS must be > 0\n");                    \
      break;                                                                 \
    }                                                                        \
    if (sb_warmup < 0) {                                                     \
      fprintf(stderr, "BENCH_CSV_N: WARMUP must be >= 0\n");                 \
      break;                                                                 \
    }                                                                        \
    FILE* sb_csv = fopen((FILE_NAME), "a+");                                 \
    if (sb_csv == NULL) {                                                    \
      fprintf(stderr, "BENCH_CSV_N: fopen(%s) failed: %s\n", (FILE_NAME),    \
              strerror(errno));                                              \
      break;                                                                 \
    }                                                                        \
    if (simple_bench_csv_write_header_if_empty(sb_csv) != 0) {               \
      fclose(sb_csv);                                                        \
      break;                                                                 \
    }                                                                        \
    for (int sb_i = 0; sb_i < sb_warmup; ++sb_i) {                           \
      FCALL;                                                                 \
    }                                                                        \
    for (int sb_i = 0; sb_i < sb_runs; ++sb_i) {                             \
      simple_bench_measurement sb_result;                                    \
      CPUTIME_RSS_USR_SYS(FCALL, &sb_result.wall_time_ns,                    \
                          &sb_result.peak_rss_delta,                         \
                          &sb_result.user_time_ns,                           \
                          &sb_result.system_time_ns);                        \
      simple_bench_csv_append_row(sb_csv, #FCALL, __FILE__, __LINE__,        \
                                  (COMMENT), &sb_result);                    \
    }                                                                        \
    fclose(sb_csv);                                                          \
  } while (0)

// Auto-calibrating benchmark. Repeatedly doubles the inner iteration count
// until a single batch runs at least MIN_TOTAL_NS nanoseconds, then collects
// BATCHES additional batches at that iteration count and reports the
// distribution of per-iteration wall time. Useful when FCALL is too fast to
// time meaningfully on its own (a few ns).
//
// Args:
//   FCALL         : statement or expression to benchmark.
//   MIN_TOTAL_NS  : minimum wall time per batch (e.g. 100000000 = 100 ms).
//   BATCHES       : how many timed batches to collect after calibration
//                   (must be > 0). 5 is a sensible default.
//
// Safety: caps the iteration count at 1<<30 to avoid runaway loops if FCALL
// is somehow elided to zero cost by the optimizer. Use SB_DO_NOT_OPTIMIZE
// inside FCALL to prevent that.
#define BENCH_AUTO(FCALL, MIN_TOTAL_NS, BATCHES)                             \
  do {                                                                       \
    long long sb_min_total = (long long)(MIN_TOTAL_NS);                      \
    int sb_batches = (BATCHES);                                              \
    if (sb_min_total <= 0) {                                                 \
      fprintf(stderr, "BENCH_AUTO: MIN_TOTAL_NS must be > 0\n");             \
      break;                                                                 \
    }                                                                        \
    if (sb_batches <= 0) {                                                   \
      fprintf(stderr, "BENCH_AUTO: BATCHES must be > 0\n");                  \
      break;                                                                 \
    }                                                                        \
    /* one warmup call */                                                    \
    FCALL;                                                                   \
    long long sb_iters = 1;                                                  \
    long long sb_batch_ns = 0;                                               \
    const long long sb_iters_cap = (long long)1 << 30;                       \
    for (;;) {                                                               \
      struct timespec sb_ts_start, sb_ts_end;                                \
      clock_gettime(CLOCK_MONOTONIC, &sb_ts_start);                          \
      for (long long sb_k = 0; sb_k < sb_iters; ++sb_k) {                    \
        FCALL;                                                               \
      }                                                                      \
      clock_gettime(CLOCK_MONOTONIC, &sb_ts_end);                            \
      sb_batch_ns = (sb_ts_end.tv_sec - sb_ts_start.tv_sec) * 1000000000LL + \
                    (sb_ts_end.tv_nsec - sb_ts_start.tv_nsec);               \
      if (sb_batch_ns >= sb_min_total || sb_iters >= sb_iters_cap) {         \
        break;                                                               \
      }                                                                      \
      sb_iters *= 2;                                                         \
    }                                                                        \
    long long* sb_samples =                                                  \
        (long long*)malloc((size_t)sb_batches * sizeof(long long));          \
    if (sb_samples == NULL) {                                                \
      fprintf(stderr, "BENCH_AUTO: malloc failed for %d batches\n",          \
              sb_batches);                                                   \
      break;                                                                 \
    }                                                                        \
    for (int sb_b = 0; sb_b < sb_batches; ++sb_b) {                          \
      struct timespec sb_ts_start, sb_ts_end;                                \
      clock_gettime(CLOCK_MONOTONIC, &sb_ts_start);                          \
      for (long long sb_k = 0; sb_k < sb_iters; ++sb_k) {                    \
        FCALL;                                                               \
      }                                                                      \
      clock_gettime(CLOCK_MONOTONIC, &sb_ts_end);                            \
      long long sb_total =                                                   \
          (sb_ts_end.tv_sec - sb_ts_start.tv_sec) * 1000000000LL +           \
          (sb_ts_end.tv_nsec - sb_ts_start.tv_nsec);                         \
      sb_samples[sb_b] = sb_total / sb_iters;                                \
    }                                                                        \
    simple_bench_stats sb_stats;                                             \
    simple_bench_compute_stats(sb_samples, sb_batches, 1, &sb_stats);        \
    print_benchmark_auto_summary(#FCALL, sb_iters, sb_batches, &sb_stats);   \
    free(sb_samples);                                                        \
  } while (0)

#ifdef __cplusplus
}  // extern "C"
#endif

#endif  // SIMPLE_BENCH_H
