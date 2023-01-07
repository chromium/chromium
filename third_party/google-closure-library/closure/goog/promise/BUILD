load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "nativeresolver",
    srcs = ["nativeresolver.js"],
)

closure_js_library(
    name = "promise",
    srcs = [
        "promise.js",
        "resolver.js",
        "thenable.js",
    ],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/async:freelist",
        "//closure/goog/async:nexttick",
        "//closure/goog/async:run",
        "//closure/goog/debug:error",
    ],
)

alias(
    name = "resolver",
    actual = ":promise",
)

closure_js_library(
    name = "testsuiteadapter",
    testonly = 1,
    srcs = ["testsuiteadapter.js"],
    deps = [":promise"],
)

alias(
    name = "thenable",
    actual = ":promise",
)
