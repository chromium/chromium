load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "pubsub",
    srcs = ["pubsub.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/async:run",
        "//closure/goog/disposable",
    ],
)

closure_js_library(
    name = "topicid",
    srcs = ["topicid.js"],
)

closure_js_library(
    name = "typedpubsub",
    srcs = ["typedpubsub.js"],
    deps = [
        ":pubsub",
        ":topicid",
        "//closure/goog/disposable",
    ],
)
