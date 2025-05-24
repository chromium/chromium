load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "all_js",
    srcs = [],
    lenient = True,
    exports = [
        ":deferred",
        ":deferredlist",
    ],
)

closure_js_library(
    name = "deferred",
    srcs = ["deferred.js"],
    lenient = True,
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/debug:error",
        "//closure/goog/promise",
    ],
)

closure_js_library(
    name = "deferredlist",
    srcs = ["deferredlist.js"],
    lenient = True,
    deps = [":deferred"],
)
