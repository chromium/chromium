load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "mock",
    srcs = ["mock.js"],
    lenient = True,
    deps = [
        ":timeoutmode",
        ":verificationmode",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/debug",
        "//closure/goog/debug:error",
        "//closure/goog/functions",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "timeoutmode",
    srcs = ["timeoutmode.js"],
    lenient = True,
)

closure_js_library(
    name = "verificationmode",
    srcs = ["verificationmode.js"],
    lenient = True,
)
