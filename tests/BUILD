#
# Copyright (c) 2025 @tragisch <https://github.com/tragisch>
# SPDX-License-Identifier: MIT
#
# This file is part of a project licensed under the MIT License.
#  See the LICENSE file in the root directory for details.
#

load("//:tools/unity_test.bzl", "unity_test")

unity_test(
    name = "test_simple_bench",
    srcs = ["test_simple_bench.c"],
    my_config = ["my_config.yml"],
    deps = [
        "//src:simple_bench",
    ],
)
