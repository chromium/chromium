load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "nativeresolver",
    srcs = ["nativeresolver.js"],
    lenient = True,
)

closure_js_library(
    name = "promise",
    srcs = [
        "promise.js",
        "resolver.js",
        "thenable.js",
    ],
    lenient = True,
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/async:freelist",
        "//closure/goog/async:run",
        "//closure/goog/async:throwexception",
        "//closure/goog/debug:asyncstacktag",
        "//closure/goog/debug:error",
        "//closure/goog/functions",
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
    lenient = True,
    deps = [":promise"],
)

alias(
    name = "thenable",
    actual = ":promise",
)
