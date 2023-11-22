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

# A few macros that prints and returns the value of a given expression
# for quick and dirty debugging, inspired by Rusts dbg!(…) macro and its C++ variant.
# https://github.com/eerimoq/dbg-macro
def load_dbg_marco():
    http_archive(
        name = "dbg-macro",
        build_file = "@//:tools/dbg-macro/BUILD",
        sha256 = "2cd05a0ab0c93d115bf0ee476a5746189f3ced1d589abb098307daeaa57ef329",
        strip_prefix = "dbg-macro-0.12.1",
        url = "https://github.com/eerimoq/dbg-macro/archive/refs/tags/0.12.1.zip",
    )

##################################
## Load C Third Party Libraries ##

# load all C repositories
def third_party_repositories():
    load_unity()
    load_dbg_marco()
