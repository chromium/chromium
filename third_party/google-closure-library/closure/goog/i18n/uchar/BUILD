load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "localnamefetcher",
    srcs = ["localnamefetcher.js"],
    lenient = True,
    deps = [
        ":namefetcher",
        "//closure/goog/i18n:ucharnames",
        "//closure/goog/log",
    ],
)

closure_js_library(
    name = "namefetcher",
    srcs = ["namefetcher.js"],
    lenient = True,
)

closure_js_library(
    name = "remotenamefetcher",
    srcs = ["remotenamefetcher.js"],
    lenient = True,
    deps = [
        ":namefetcher",
        "//closure/goog/disposable",
        "//closure/goog/events",
        "//closure/goog/i18n:uchar",
        "//closure/goog/log",
        "//closure/goog/net:eventtype",
        "//closure/goog/net:xhrio",
        "//closure/goog/uri",
    ],
)
