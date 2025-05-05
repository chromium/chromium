load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "layoutasserts",
    testonly = True,
    srcs = ["layoutasserts.js"],
    lenient = True,
    deps = [
        ":style",
        "//closure/goog/style",
        "//closure/goog/testing:asserts",
    ],
)

closure_js_library(
    name = "style",
    testonly = True,
    srcs = ["style.js"],
    lenient = True,
    deps = [
        "//closure/goog/dom",
        "//closure/goog/math:rect",
        "//closure/goog/style",
    ],
)
