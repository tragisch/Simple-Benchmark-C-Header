/*
 * Example using BENCH macro unsing simple sleep
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "simple_bench.h"

void sleep_some_time(int seconds, int memory_usage) {
  const int MEGABYTE = 1024 * 1024;
  const int MAX_RANDOM_VALUE = 255;
  char *store_some_memory = malloc((unsigned long)memory_usage * MEGABYTE);
  sleep(seconds);
  free(store_some_memory);
}

int main() {
  BENCH(sleep_some_time(3, 2000));
  return 0;
}