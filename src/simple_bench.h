#ifndef SIMPLE_BENCH_H
#define SIMPLE_BENCH_H

#include <stdio.h>
#include <sys/resource.h>
#include <time.h>

#ifdef __MACH__  // For macOS, as it doesn't have clock_gettime by default
#include <mach/clock.h>
#include <mach/mach.h>
#endif

/*****************************
 *  some helper
 *******************************/

// Define the color codes
#define BLUE "\x1B[34m"
#define RED "\x1B[31m"
#define WHITE "\x1B[37m"
#define RESET "\x1B[0m"

// human readable time format
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

// human readable memory format
static inline void format_memory_usage(long memory_usage, char* buffer,
                                       size_t buffer_size) {
  const long KB = 1024;       // Kilobytes in a Megabyte
  const long MB = 1024 * KB;  // Kilobytes in a Gigabyte

#ifdef __linux__
  // On Linux, ru_maxrss is in kilobytes
  long memory_kilobytes = memory_usage;
#else
  // On BSD and macOS, ru_maxrss is in bytes
  long memory_kilobytes = memory_usage / KB;
#endif

  if (memory_kilobytes < KB) {
    snprintf(buffer, buffer_size, "%ld KB", memory_kilobytes);
  } else if (memory_kilobytes < MB) {
    snprintf(buffer, buffer_size, "%.2f MB", memory_kilobytes / (double)KB);
  } else {
    snprintf(buffer, buffer_size, "%.2f GB", memory_kilobytes / (double)MB);
  }
}

// Function to print benchmark result
static inline void print_benchmark_result(const char* formatted_time,
                                          const long long memory,
                                          const char* fcall, const char* file,
                                          int line) {
  char formatted_memory[64];
  format_memory_usage(memory, formatted_memory, sizeof(formatted_memory));
  printf(BLUE "ℬ|" RESET " CPU-Time: " RED "%s\t" RESET "RSS:" RED
              " ~ %s\t" RESET "| %s in " WHITE " %s: % i\n",
         formatted_time, formatted_memory, fcall, file, line);
}

static inline void update_countdown(int remaining) {
  printf("\r" BLUE "ℬ|" RESET "%i ", remaining);
  fflush(stdout);
}

static inline void clear_line() { printf("\r\033[K"); }

static inline long get_memory_usage() {
  struct rusage usage;
  getrusage(RUSAGE_SELF, &usage);
  return usage
      .ru_maxrss;  // Returns the maximum resident set size used (in kilobytes)
}

// write double date to file in a table woth row "name" and colum format
static inline void write_double_to_file(char* filename,
                                        const char* function_name,
                                        const char* time_usage,
                                        const char* memory_usage) {
  FILE* file = NULL;
  file = fopen(filename, "a");
  if (file == NULL) {
    printf("Error opening file!\n");
    exit(1);
  }
  time_t tm = 0;
  time(&tm);
  fprintf(file, "%s\t%s\t%s\t%s\n", function_name, time_usage, memory_usage,
          ctime(&tm));
  fclose(file);
}

/*****************************
 *  BENCH MACROs
 *******************************/

#define BENCH(FCALL)                                                      \
  do {                                                                    \
    long long elapsed, memory_used;                                       \
    CPUTIME_RSS(FCALL, &elapsed, &memory_used);                           \
    char formatted_time[64];                                              \
    format_duration(elapsed, formatted_time, sizeof(formatted_time));     \
    print_benchmark_result(formatted_time, memory_used, #FCALL, __FILE__, \
                           __LINE__);                                     \
  } while (0)

// CPUTIME macro
// be careful that FCALL is NOT an expression.
#define CPUTIME(FCALL)                                  \
  ({                                                    \
    struct timespec start, end;                         \
    long long elapsed;                                  \
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {  \
      perror("clock_gettime");                          \
      elapsed = -1;                                     \
    } else {                                            \
      FCALL;                                            \
      if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {  \
        perror("clock_gettime");                        \
        elapsed = -1;                                   \
      } else {                                          \
        elapsed = (end.tv_sec - start.tv_sec) * 1.0e9 + \
                  (end.tv_nsec - start.tv_nsec);        \
      }                                                 \
    }                                                   \
    elapsed;                                            \
  })

// CPUTIME_RSS macro
#define CPUTIME_RSS(FCALL, TIME_PTR, RSS_PTR)             \
  do {                                                    \
    struct timespec start, end;                           \
    long memory_before, memory_after;                     \
                                                          \
    memory_before = get_memory_usage();                   \
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {    \
      perror("clock_gettime");                            \
      *TIME_PTR = -1;                                     \
    } else {                                              \
      FCALL;                                              \
      if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {    \
        perror("clock_gettime");                          \
        *TIME_PTR = -1;                                   \
      } else {                                            \
        *TIME_PTR = (end.tv_sec - start.tv_sec) * 1.0e9 + \
                    (end.tv_nsec - start.tv_nsec);        \
      }                                                   \
    }                                                     \
    memory_after = get_memory_usage();                    \
    *RSS_PTR = memory_after - memory_before;              \
  } while (0)

#define RSS(FCALL)                                   \
  ({                                                 \
    long memory_before = get_memory_usage();         \
    FCALL;                                           \
    long memory_after = get_memory_usage();          \
    long memory_used = memory_after - memory_before; \
  })

#endif  // SIMPLE_BENCH_H