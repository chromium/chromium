load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "boundedcollectablestorage",
    srcs = ["boundedcollectablestorage.js"],
    lenient = True,
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/iter",
        "//closure/goog/storage:collectablestorage",
        "//closure/goog/storage:errorcode",
        "//closure/goog/storage:expiringstorage",
        "//closure/goog/storage/mechanism:iterablemechanism",
    ],
)
