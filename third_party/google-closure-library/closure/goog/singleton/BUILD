load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "singleton",
    srcs = ["singleton.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/reflect",
    ],
)
