/*
 * Example using BENCH macro: simple sleep function and memory allocation
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "simple_bench.h"

void sleep_some_time(int seconds, int memory_usage) {
  const int MEGABYTE = 1024 * 1024;
  char *store_some_memory = malloc((unsigned long)memory_usage * MEGABYTE);
  sleep(seconds);
  free(store_some_memory);
}

int main() {
  BENCH(sleep_some_time(3, 20));
  return 0;
}