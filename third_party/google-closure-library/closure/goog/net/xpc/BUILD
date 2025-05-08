load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "crosspagechannel",
    srcs = [
        "crosspagechannel.js",
        "nativemessagingtransport.js",
    ],
    lenient = True,
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
        "//closure/goog/functions",
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
    lenient = True,
)

alias(
    name = "nativemessagingtransport",
    actual = ":crosspagechannel",
)

closure_js_library(
    name = "relay",
    srcs = ["relay.js"],
    lenient = True,
)

closure_js_library(
    name = "transport",
    srcs = ["transport.js"],
    lenient = True,
    deps = [
        ":xpc",
        "//closure/goog/disposable",
        "//closure/goog/dom",
    ],
)

closure_js_library(
    name = "xpc",
    srcs = ["xpc.js"],
    lenient = True,
    deps = ["//closure/goog/log"],
)
