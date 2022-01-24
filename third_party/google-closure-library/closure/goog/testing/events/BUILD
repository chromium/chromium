load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "eventobserver",
    srcs = ["eventobserver.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/events:event",
        "//closure/goog/events:eventid",
    ],
)

closure_js_library(
    name = "events",
    srcs = ["events.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/disposable",
        "//closure/goog/dom:nodetype",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:browserfeature",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/events:keycodes",
        "//closure/goog/math:coordinate",
        "//closure/goog/object",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "matchers",
    srcs = ["matchers.js"],
    deps = [
        "//closure/goog/events:event",
        "//closure/goog/testing:mockmatchers",
    ],
)

closure_js_library(
    name = "onlinehandler",
    srcs = ["onlinehandler.js"],
    deps = [
        "//closure/goog/events:eventtarget",
        "//closure/goog/net:networkstatusmonitor",
    ],
)
