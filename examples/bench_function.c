/*
 * Example using BENCH macro unsing simple sleep
 */
#include <stdio.h>
#include <stdlib.h>
#include <time.h>
#include <unistd.h>

#include "simple_bench.h"

void sleep_some_time(int seconds, int memory_usage) {
  sleep(seconds);
  // allocate 2MB of memory

  const int MEGABYTE = 1024 * 1024;
  const int MAX_RANDOM_VALUE = 255;
  char *store_some_memory = malloc((unsigned long)memory_usage * MEGABYTE);
  // fill the memory with random data
  for (int i = 0; i < memory_usage * MEGABYTE; i++) {
    store_some_memory[i] = (char)(arc4random() % MAX_RANDOM_VALUE);
  }
  // free the memory
  free(store_some_memory);
}

int main() {
  BENCH(sleep_some_time(3, 4));
  return 0;
}