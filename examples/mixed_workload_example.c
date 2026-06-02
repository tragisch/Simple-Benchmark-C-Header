#include "simple_bench.h"

#include <stdlib.h>

static int compare_ints(const void* lhs, const void* rhs) {
  const int a = *(const int*)lhs;
  const int b = *(const int*)rhs;
  return (a > b) - (a < b);
}

static void mixed_workload(void) {
  const size_t count = 200000U;
  int* data = (int*)malloc(count * sizeof(int));
  if (data == NULL) {
    return;
  }

  unsigned int state = 123456789U;
  for (size_t i = 0; i < count; ++i) {
    state = (1103515245U * state + 12345U);
    data[i] = (int)(state & 0x7FFFFFFFU);
  }

  qsort(data, count, sizeof(int), compare_ints);

  volatile long long checksum = 0;
  for (size_t i = 0; i < count; i += 1000U) {
    checksum += data[i];
  }
  (void)checksum;

  free(data);
}

int main(void) {
  BENCH(mixed_workload());
  BENCH_VERBOSE(mixed_workload());
  return 0;
}
