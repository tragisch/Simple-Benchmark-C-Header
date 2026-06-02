#include "simple_bench.h"

static void cpu_bound_work(void) {
  volatile unsigned long long sum = 0;
  for (unsigned long long i = 0; i < 50000000ULL; ++i) {
    sum += (i % 97ULL) * (i % 31ULL);
  }
  (void)sum;
}

int main(void) {
  BENCH(cpu_bound_work());
  BENCH_VERBOSE(cpu_bound_work());
  BENCH_N(cpu_bound_work(), 5, 1);
  BENCH_CSV(cpu_bound_work(), "cpu_bound_results.csv", "example-run");
  return 0;
}
