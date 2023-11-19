#ifndef SIMPLE_BENCH_H
#define SIMPLE_BENCH_H

#include <stdio.h>
#include <time.h>

#ifdef __MACH__  // For macOS, as it doesn't have clock_gettime by default
#include <mach/clock.h>
#include <mach/mach.h>
#endif

/*****************************
 *  some helper
 *******************************/

// TODO: on windows colors are not supported:
#define RED "\x1B[31m"
#define GREEN "\x1B[32m"
#define YELLOW "\x1B[33m"
#define BLUE "\x1B[34m"
#define MAGENTA "\x1B[35m"
#define CYAN "\x1B[36m"
#define WHITE "\x1B[97m"
#define RESET "\x1B[0m"

// human readable time format
void format_duration(long long nanoseconds, char* buffer, size_t buffer_size) {
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

/*****************************
 *  BENCH MACROs
 *******************************/

#define BENCH(FCALL, ITERATIONS)                                    \
  do {                                                              \
    double total_elapsed = 0.0;                                     \
    char formatted_time[64];                                        \
    for (int i = 0; i < ITERATIONS; i++) {                          \
      double elapsed = CPUTIME(FCALL);                              \
      printf("\r" BLUE "ℬ|" RESET "%i", ITERATIONS - i);            \
      fflush(stdout);                                               \
      total_elapsed += elapsed;                                     \
    }                                                               \
    printf("\r");                                                   \
    format_duration(total_elapsed / ITERATIONS, formatted_time,     \
                    sizeof(formatted_time));                        \
    printf(BLUE "ℬ|" RESET " CPU-Time: " RED "%s\t" RESET           \
                "Iterations: %i\t | %s in " WHITE "%s:%i\n",        \
           formatted_time, ITERATIONS, #FCALL, __FILE__, __LINE__); \
  } while (0)

#define S_BENCH(FCALL)                                                \
  do {                                                                \
    double elapsed = CPUTIME(FCALL);                                  \
    char formatted_time[64];                                          \
    format_duration(elapsed, formatted_time, sizeof(formatted_time)); \
    printf(BLUE "ℬ|" RESET " CPU-Time: " RED "%s\t" RESET             \
                "Iterations: %i\t | %s in " WHITE "%s:%i\n",          \
           formatted_time, 1, #FCALL, __FILE__, __LINE__);            \
  } while (0)

// CPUTIME macro, return time in nanoseconds to use further
#define CPUTIME(FCALL)                                  \
  ({                                                    \
    struct timespec start, end;                         \
    double elapsed;                                     \
    if (clock_gettime(CLOCK_MONOTONIC, &start) != 0) {  \
      perror("clock_gettime");                          \
      elapsed = -1.0;                                   \
    } else {                                            \
      FCALL;                                            \
      if (clock_gettime(CLOCK_MONOTONIC, &end) != 0) {  \
        perror("clock_gettime");                        \
        elapsed = -1.0;                                 \
      } else {                                          \
        elapsed = (end.tv_sec - start.tv_sec) * 1.0e9 + \
                  (end.tv_nsec - start.tv_nsec);        \
      }                                                 \
    }                                                   \
    elapsed;                                            \
  })

#endif  // SIMPLE_BENCH_H