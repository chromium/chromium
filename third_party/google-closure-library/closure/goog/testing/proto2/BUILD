load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "proto2",
    testonly = True,
    srcs = ["proto2.js"],
    lenient = True,
    deps = [
        "//closure/goog/proto2:message",
        "//closure/goog/proto2:objectserializer",
        "//closure/goog/testing:asserts",
    ],
)
