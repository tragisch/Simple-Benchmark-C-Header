# Simple Benchmark C-Header
An C-header only library for a simple benchmark of a function or code_block.
One macros is (currently) provided:

BENCH(FCALL) 

where FCALL is a function call or a code block.
The macro will print the (CPU) time it took to execute the function call or code block.
And it returns from "rusage" a snapshot of the current resource usage (RSS) of the function. Of course it is more a hint of the real memory comsunption of the function.

It should work on Linux and MacOS.

## Usage

```c
int main() {

  BENCH(countTuesdays(3,4));

  return 0;
}
```
results in:
![Alt text](image1.png)

## Installation
Just use the header file. 

Or if you would like to build the example, you can clone the repository and build it with Bazel:
```bash
bazel build //examples:bench_1
```
