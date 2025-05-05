load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "basicstat",
    srcs = ["basicstat.js"],
    lenient = True,
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/log",
        "//closure/goog/string:stringformat",
        "//closure/goog/structs:circularbuffer",
    ],
)
