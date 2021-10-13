load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "interpolator1",
    srcs = ["interpolator1.js"],
)

closure_js_library(
    name = "linear1",
    srcs = ["linear1.js"],
    deps = [
        ":interpolator1",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/math",
    ],
)

closure_js_library(
    name = "pchip1",
    srcs = ["pchip1.js"],
    deps = [
        ":spline1",
        "//closure/goog/math",
    ],
)

closure_js_library(
    name = "spline1",
    srcs = ["spline1.js"],
    deps = [
        ":interpolator1",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/math",
        "//closure/goog/math:tdma",
    ],
)
