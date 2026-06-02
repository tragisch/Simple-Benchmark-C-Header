#include "simple_bench.h"

#include <stdlib.h>

static void memory_bound_work(void) {
  const size_t count = 2U * 1024U * 1024U;
  int* data = (int*)malloc(count * sizeof(int));
  if (data == NULL) {
    return;
  }

  for (size_t i = 0; i < count; ++i) {
    data[i] = (int)(i & 0xFFU);
  }

  volatile long long total = 0;
  for (size_t i = 0; i < count; ++i) {
    total += data[i];
  }
  (void)total;

  free(data);
}

int main(void) {
  BENCH(memory_bound_work());
  BENCH_VERBOSE(memory_bound_work());
  return 0;
}
