load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "fakewebchannel",
    testonly = True,
    srcs = ["fakewebchannel.js"],
    lenient = True,
    deps = [
        "//closure/goog/array",
        "//closure/goog/events",
        "//closure/goog/labs/net:webchannel",
        "//closure/goog/testing:asserts",
    ],
)
