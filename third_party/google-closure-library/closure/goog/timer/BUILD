load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "timer",
    srcs = ["timer.js"],
    deps = [
        "//closure/goog/events:eventtarget",
        "//closure/goog/promise",
    ],
)
