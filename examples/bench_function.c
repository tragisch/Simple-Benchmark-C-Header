#include <stdio.h>
#include <time.h>

#include "simple_bench.h"

// Just a function to benchmark:
// Count the number of Tuesdays in the 21st century
int countTuesdays() {
  int count = 0;
  struct tm timeinfo = {.tm_year = 100, .tm_mon = 0, .tm_mday = 1};

  time_t rawtime;
  for (int i = 0; i < 36525; i++) {
    rawtime = mktime(&timeinfo);
    if (timeinfo.tm_wday == 2) {  // Tuesday is represented by 2 in tm_wday
      count++;
    }
    rawtime += 86400;  // Add 24 hours in seconds
    timeinfo = *localtime(&rawtime);
  }

  return count;
}

int main() {
  // Benchmark the function 15 times
  BENCH(countTuesdays(), 15);

  // Single interation benchmark
  S_BENCH(countTuesdays());

  return 0;
}