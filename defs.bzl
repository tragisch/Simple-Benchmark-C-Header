load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

# Simple Unit Testing for C
# https://github.com/ThrowTheSwitch/Unity/archive/v2.5.2.zip
# LICENSE: MIT
def load_unity():
    http_archive(
        name = "Unity",
        build_file = "@//:tools/ThrowTheSwitch/Unity/BUILD",
        sha256 = "4598298723ecca1f242b8c540a253ae4ab591f6810cbde72f128961007683034",
        strip_prefix = "Unity-2.5.2",
        urls = [
            "https://github.com/ThrowTheSwitch/Unity/archive/refs/tags/v2.5.2.zip",
        ],
    )

##################################
## Load C Third Party Libraries ##

# load all C repositories
def third_party_repositories():
    load_unity()
