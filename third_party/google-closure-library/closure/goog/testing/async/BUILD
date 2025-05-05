load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "mockcontrol",
    testonly = True,
    srcs = ["mockcontrol.js"],
    lenient = True,
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/debug",
        "//closure/goog/testing:asserts",
        "//closure/goog/testing:mockcontrol",
        "//closure/goog/testing:mockmatchers",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)
