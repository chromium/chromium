load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "proto",
    srcs = ["proto.js"],
    deps = [":serializer"],
)

closure_js_library(
    name = "serializer",
    srcs = ["serializer.js"],
    deps = [
        "//closure/goog/json",
        "//closure/goog/string",
    ],
)
