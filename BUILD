# library:
cc_library(
    name = "bench",
    srcs = ["src/simple_bench.h"],
    hdrs = ["src/simple_bench.h"],
    includes = ["src"],
    visibility = ["//visibility:public"],
)

# example:
cc_binary(
    name = "bench_1",
    srcs = ["examples/bench_function.c"],
    visibility = ["//visibility:public"],
    deps = [
        "//:bench",
    ],
)
