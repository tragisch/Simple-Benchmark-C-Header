load("@rules_cc//cc:defs.bzl", "cc_library")

filegroup(
    name = "TestRunnerGenerator",
    srcs = ["auto/generate_test_runner.rb"],
    visibility = ["//visibility:public"],
)

filegroup(
    name = "HelperScripts",
    srcs = [
        "auto/run_test.erb",
        "auto/type_sanitizer.rb",
        "auto/unity_test_summary.rb",
    ],
    visibility = ["//visibility:public"],
)

cc_library(
    name = "Unity",
    srcs = ["src/unity.c"],
    hdrs = [
        "src/unity.h",
        "src/unity_internals.h",
    ],
    includes = ["src"],
    visibility = ["//visibility:public"],
)
