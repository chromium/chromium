load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "asserts",
    testonly = 1,
    srcs = ["asserts.js"],
    deps = ["//closure/goog/testing:jsunit"],
)
