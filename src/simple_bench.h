#ifndef SIMPLE_BENCH_H
#define SIMPLE_BENCH_H

#include <limits.h>
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <sys/resource.h>
#include <sys/time.h>
#include <sys/utsname.h>
#include <time.h>
#include <unistd.h>

// Define the color codes
#define BLUE "\x1B[34m"
#define RED "\x1B[31m"
#define WHITE "\x1B[37m"
#define RESET "\x1B[0m"

typedef struct simple_bench_measurement {
  long long wall_time_ns;
  long long user_time_ns;
  long long system_time_ns;
  long peak_rss_delta;
} simple_bench_measurement;

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
  printf("B| %s | wall=%s | peak-rss-delta=%s\n", fcall, formatted_wall,
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
  printf(BLUE "ℬ|" RESET " Wall-Time: " RED "%s\t" RESET "User: " RED "%s\t"
              RESET "System: " RED "%s\t" RESET "Peak-RSS-Delta: " RED "%s\t"
              RESET "| %s in " WHITE "%s:%i\n",
         formatted_wall, formatted_user, formatted_system, formatted_memory,
         fcall, file, line);
}

static inline void print_benchmark_stats_summary(const char* fcall, int runs,
                                                 int warmup, long long min_ns,
                                                 long long max_ns,
                                                 long long mean_ns) {
  char min_buf[64];
  char max_buf[64];
  char mean_buf[64];
  format_duration(min_ns, min_buf, sizeof(min_buf));
  format_duration(max_ns, max_buf, sizeof(max_buf));
  format_duration(mean_ns, mean_buf, sizeof(mean_buf));
  printf("B| %s | n=%d warmup=%d | wall[min/mean/max]=%s / %s / %s\n", fcall,
         runs, warmup, min_buf, mean_buf, max_buf);
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

static inline void write_benchmark_csv_row(const char* filename,
                                           const char* function_name,
                                           const char* file_name, int line,
                                           const char* comment,
                                           const simple_bench_measurement* m) {
  FILE* file = fopen(filename, "a+");
  if (file == NULL) {
    perror("fopen");
    exit(1);
  }

  if (fseek(file, 0, SEEK_END) != 0) {
    perror("fseek");
    fclose(file);
    exit(1);
  }
  long file_size = ftell(file);
  if (file_size < 0) {
    perror("ftell");
    fclose(file);
    exit(1);
  }
  if (file_size == 0) {
    fprintf(file,
            "function,wall_time_ns,user_time_ns,system_time_ns,peak_rss_delta,"
            "file,line,timestamp,comment\n");
  }

  time_t now = time(NULL);
  struct tm local_time_value;
  struct tm* local_time = localtime_r(&now, &local_time_value);
  if (local_time == NULL) {
    perror("localtime_r");
    fclose(file);
    exit(1);
  }

  char timestamp[32];
  if (strftime(timestamp, sizeof(timestamp), "%Y-%m-%dT%H:%M:%S", local_time) ==
      0) {
    perror("strftime");
    fclose(file);
    exit(1);
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

  fclose(file);
}

// Kept for backward compatibility with older TSV-based workflows.
static inline void write_double_to_file(char* filename,
                                        const char* function_name,
                                        const char* time_usage,
                                        const char* memory_usage,
                                        const char* comment) {
  FILE* file = NULL;
  file = fopen(filename, "a");
  if (file == NULL) {
    perror("fopen");
    exit(1);
  }

  struct utsname sysinfo;
  if (uname(&sysinfo) != 0) {
    strcpy(sysinfo.sysname, "UnknownOS");
  }

  char hostname[256];
  if (gethostname(hostname, sizeof(hostname)) != 0) {
    strcpy(hostname, "UnknownHost");
  }

  time_t tm = 0;
  time(&tm);
  fprintf(file, "%s\t%s\t%s\t%s\t%s\t%s\t%s\n", function_name, time_usage,
          memory_usage, comment, sysinfo.sysname, hostname, ctime(&tm));
  fclose(file);
}

static inline void print_environment_info(void) {
  struct utsname sysinfo;
  if (uname(&sysinfo) == 0) {
    printf("\r" BLUE "ℬ|" RESET " OS: %s %s (%s)", sysinfo.sysname,
           sysinfo.release, sysinfo.machine);
  }

  long nprocs = sysconf(_SC_NPROCESSORS_ONLN);
  if (nprocs > 0) {
    printf(" | Cores: %ld", nprocs);
  }

  long pages = sysconf(_SC_PHYS_PAGES);
  long page_size = sysconf(_SC_PAGE_SIZE);
  if (pages > 0 && page_size > 0) {
    double mem_gb = (pages * page_size) / (1024.0 * 1024.0 * 1024.0);
    printf(" | RAM: %.2f GB", mem_gb);
  }

  double loadavg[3];
  if (getloadavg(loadavg, 3) == 3) {
    printf(" | Load: %.2f / %.2f / %.2f", loadavg[0], loadavg[1], loadavg[2]);
  }

  printf("\n");
  fflush(stdout);
}

// CPUTIME macro (returns wall-clock elapsed nanoseconds for FCALL).
#define CPUTIME(FCALL)                                                       \
  __extension__({                                                            \
    struct timespec start, end;                                              \
    long long elapsed;                                                       \
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {                       \
      perror("clock_gettime");                                               \
      elapsed = -1;                                                          \
    } else {                                                                 \
      FCALL;                                                                 \
      if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {                       \
        perror("clock_gettime");                                             \
        elapsed = -1;                                                        \
      } else {                                                               \
        elapsed = (end.tv_sec - start.tv_sec) * 1000000000LL +               \
                  (end.tv_nsec - start.tv_nsec);                             \
      }                                                                      \
    }                                                                        \
    elapsed;                                                                 \
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
    long memory_before = get_memory_usage();                                 \
    FCALL;                                                                   \
    long memory_after = get_memory_usage();                                  \
    long memory_used = memory_after - memory_before;                         \
    memory_used;                                                             \
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

#define BENCH_N(FCALL, RUNS, WARMUP)                                          \
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
    long long sb_min = LLONG_MAX;                                            \
    long long sb_max = LLONG_MIN;                                            \
    long long sb_sum = 0;                                                    \
    for (int sb_i = 0; sb_i < sb_runs; ++sb_i) {                             \
      simple_bench_measurement sb_result;                                    \
      CPUTIME_RSS_USR_SYS(FCALL, &sb_result.wall_time_ns,                    \
                          &sb_result.peak_rss_delta, &sb_result.user_time_ns, \
                          &sb_result.system_time_ns);                        \
      if (sb_result.wall_time_ns < sb_min) {                                 \
        sb_min = sb_result.wall_time_ns;                                     \
      }                                                                      \
      if (sb_result.wall_time_ns > sb_max) {                                 \
        sb_max = sb_result.wall_time_ns;                                     \
      }                                                                      \
      sb_sum += sb_result.wall_time_ns;                                      \
    }                                                                        \
    print_benchmark_stats_summary(#FCALL, sb_runs, sb_warmup, sb_min, sb_max, \
                                  sb_sum / sb_runs);                         \
  } while (0)

#define BENCH_CSV(FCALL, FILE_NAME, COMMENT)                                  \
  do {                                                                       \
    simple_bench_measurement sb_result;                                      \
    CPUTIME_RSS_USR_SYS(FCALL, &sb_result.wall_time_ns, &sb_result.peak_rss_delta, \
                        &sb_result.user_time_ns, &sb_result.system_time_ns); \
    write_benchmark_csv_row((FILE_NAME), #FCALL, __FILE__, __LINE__,         \
                            (COMMENT), &sb_result);                          \
  } while (0)

#endif  // SIMPLE_BENCH_H
