load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "broadcastpubsub",
    srcs = ["broadcastpubsub.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/async:run",
        "//closure/goog/disposable",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/log",
        "//closure/goog/math",
        "//closure/goog/pubsub",
        "//closure/goog/storage",
        "//closure/goog/storage/mechanism:html5localstorage",
        "//closure/goog/string",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)
