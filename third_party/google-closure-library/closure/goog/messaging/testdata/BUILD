load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "portchannel_worker",
    srcs = ["portchannel_worker.js"],
    deps = ["//closure/goog/messaging:portchannel"],
)

closure_js_library(
    name = "portnetwork_worker1",
    srcs = ["portnetwork_worker1.js"],
    deps = [
        "//closure/goog/messaging:portcaller",
        "//closure/goog/messaging:portchannel",
    ],
)

closure_js_library(
    name = "portnetwork_worker2",
    srcs = ["portnetwork_worker2.js"],
    deps = [
        "//closure/goog/messaging:portcaller",
        "//closure/goog/messaging:portchannel",
    ],
)
