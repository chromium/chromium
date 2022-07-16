load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "es6",
    srcs = ["es6.js"],
    deps = [":iter"],
)

closure_js_library(
    name = "iter",
    srcs = ["iter.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/functions",
        "//closure/goog/math",
    ],
)
