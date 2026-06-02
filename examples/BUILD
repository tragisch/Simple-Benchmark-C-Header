load("@rules_cc//cc:defs.bzl", "cc_binary")

cc_binary(
    name = "cpu_bound_example",
    srcs = ["cpu_bound_example.c"],
    deps = ["//src:simple_bench"],
)

cc_binary(
    name = "memory_bound_example",
    srcs = ["memory_bound_example.c"],
    deps = ["//src:simple_bench"],
)

cc_binary(
    name = "mixed_workload_example",
    srcs = ["mixed_workload_example.c"],
    deps = ["//src:simple_bench"],
)
