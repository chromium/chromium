load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "animationframe",
    srcs = ["animationframe.js"],
    lenient = True,
    deps = [":polyfill"],
)

closure_js_library(
    name = "polyfill",
    srcs = ["polyfill.js"],
    lenient = True,
)
