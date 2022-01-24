load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "mockiframeio",
    srcs = ["mockiframeio.js"],
    deps = [
        "//closure/goog/events:eventtarget",
        "//closure/goog/net:errorcode",
        "//closure/goog/net:eventtype",
        "//closure/goog/net:iframeio",
        "//closure/goog/structs:map",
        "//closure/goog/testing:testqueue",
        "//closure/goog/uri",
    ],
)

closure_js_library(
    name = "xhrio",
    srcs = ["xhrio.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/collections:maps",
        "//closure/goog/dom:xml",
        "//closure/goog/events",
        "//closure/goog/net:errorcode",
        "//closure/goog/net:eventtype",
        "//closure/goog/net:httpstatus",
        "//closure/goog/net:xhrio",
        "//closure/goog/net:xhrlike",
        "//closure/goog/net:xmlhttp",
        "//closure/goog/object",
        "//closure/goog/structs",
        "//closure/goog/structs:map",
        "//closure/goog/testing:testqueue",
        "//closure/goog/uri",
        "//closure/goog/uri:utils",
    ],
)

closure_js_library(
    name = "xhriopool",
    srcs = ["xhriopool.js"],
    deps = [
        ":xhrio",
        "//closure/goog/net:xhrio",
        "//closure/goog/net:xhriopool",
    ],
)
