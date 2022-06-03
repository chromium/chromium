load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "event",
    srcs = ["event.js"],
    deps = [
        ":eventtype",
        "//closure/goog/events:event",
    ],
)

closure_js_library(
    name = "eventtype",
    srcs = ["eventtype.js"],
)

closure_js_library(
    name = "history",
    srcs = ["history.js"],
    deps = [
        ":event",
        ":eventtype",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:inputtype",
        "//closure/goog/dom:safe",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:trustedresourceurl",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/labs/useragent:device",
        "//closure/goog/memoize",
        "//closure/goog/string",
        "//closure/goog/string:const",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "html5history",
    srcs = ["html5history.js"],
    deps = [
        ":event",
        "//closure/goog/asserts",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
    ],
)
