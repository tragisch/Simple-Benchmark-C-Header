# Simple Benchmark C-Header
A C-header-only library for simple benchmarking of a function call or code block.
The following macros are available:

- `BENCH(FCALL)` for concise output (default).
- `BENCH_VERBOSE(FCALL)` for detailed output with wall/user/system/peak RSS delta.
- `BENCH_N(FCALL, runs, warmup)` for repeated wall-time stats.
- `BENCH_CSV(FCALL, "file.csv", "comment")` for machine-readable CSV rows.

`FCALL` can be a function call or a code block.
Time metrics are wall/user/system and memory is peak RSS delta from `rusage`.
Memory numbers are a practical approximation, not exact per-call allocation usage.

It should work on Linux and macOS.

## Usage
```c
#include "simple_bench.h"

void do_work(void) {
  /* your code */
}

int main() {
  BENCH(do_work());
  BENCH_VERBOSE(do_work());
  BENCH_N(do_work(), 10, 2);
  BENCH_CSV(do_work(), "bench.csv", "baseline");
  return 0;
}
```
results in:
![Alt text](image1.png)

## Installation
Use the header directly, or depend on the Bazel target:

```bash
bazel test //...
```

## Examples
Representative examples are available in `examples/`:

- `cpu_bound_example`: arithmetic-heavy loop workload.
- `memory_bound_example`: allocation + linear memory access workload.
- `mixed_workload_example`: data generation + sort + sampled reduction.

Build all examples:

```bash
bazel build //examples:all
```

Run one example:

```bash
bazel run //examples:cpu_bound_example
```

## Notes about measurements
- Time is measured using `clock_gettime(CLOCK_MONOTONIC)`.
- User/system CPU times use `getrusage` (`ru_utime` / `ru_stime`) deltas.
- Memory uses `ru_maxrss` from `getrusage`; this is peak RSS, so reported `peak_rss_delta` is an approximation.
