load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "animationdelay",
    srcs = ["animationdelay.js"],
    lenient = True,
    deps = [
        "//closure/goog/disposable",
        "//closure/goog/events",
        "//closure/goog/functions",
    ],
)

closure_js_library(
    name = "conditionaldelay",
    srcs = ["conditionaldelay.js"],
    lenient = True,
    deps = [
        ":delay",
        "//closure/goog/disposable",
    ],
)

closure_js_library(
    name = "debouncer",
    srcs = ["debouncer.js"],
    lenient = True,
    deps = [
        "//closure/goog/disposable",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "delay",
    srcs = ["delay.js"],
    lenient = True,
    deps = [
        "//closure/goog/disposable",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "freelist",
    srcs = ["freelist.js"],
    lenient = True,
)

closure_js_library(
    name = "nexttick",
    srcs = [
        "nexttick.js",
    ],
    lenient = True,
    deps = [
        "//closure/goog/debug:entrypointregistry",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/functions",
        "//closure/goog/labs/useragent:browser",
        "//closure/goog/labs/useragent:engine",
    ],
)

closure_js_library(
    name = "throwexception",
    srcs = [
        "throwexception.js",
    ],
    lenient = True,
)

closure_js_library(
    name = "promises",
    srcs = ["promises.js"],
    lenient = True,
    deps = ["//closure/goog/promise"],
)

closure_js_library(
    name = "run",
    srcs = ["run.js"],
    lenient = True,
    deps = [
        ":nexttick",
        ":throwexception",
        ":workqueue",
        "//closure/goog/asserts",
        "//closure/goog/debug:asyncstacktag",
    ],
)

closure_js_library(
    name = "throttle",
    srcs = [
        "legacy_throttle.js",
        "throttle.js",
    ],
    lenient = True,
    deps = [
        "//closure/goog/disposable",
        "//closure/goog/timer",
    ],
)

closure_js_library(
    name = "workqueue",
    srcs = ["workqueue.js"],
    lenient = True,
    deps = [
        ":freelist",
        "//closure/goog/asserts",
    ],
)
