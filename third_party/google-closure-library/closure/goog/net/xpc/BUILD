load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "crosspagechannel",
    srcs = [
        "crosspagechannel.js",
        "directtransport.js",
        "nativemessagingtransport.js",
    ],
    deps = [
        ":crosspagechannelrole",
        ":transport",
        ":xpc",
        "//closure/goog/asserts",
        "//closure/goog/async:delay",
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/html:legacyconversions",
        "//closure/goog/json",
        "//closure/goog/log",
        "//closure/goog/messaging:abstractchannel",
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/timer",
        "//closure/goog/uri",
        "//closure/goog/uri:utils",
        "//closure/goog/useragent",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "crosspagechannelrole",
    srcs = ["crosspagechannelrole.js"],
)

alias(
    name = "directtransport",
    actual = ":crosspagechannel",
)

alias(
    name = "nativemessagingtransport",
    actual = ":crosspagechannel",
)

closure_js_library(
    name = "relay",
    srcs = ["relay.js"],
)

closure_js_library(
    name = "transport",
    srcs = ["transport.js"],
    deps = [
        ":xpc",
        "//closure/goog/disposable",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "xpc",
    srcs = ["xpc.js"],
    deps = ["//closure/goog/log"],
)
