load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "cursor",
    srcs = ["cursor.js"],
    lenient = True,
    deps = [
        ":error",
        ":keyrange",
        "//closure/goog/debug",
        "//closure/goog/events:eventtarget",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "db",
    srcs = ["db.js"],
    lenient = True,
    deps = [
        ":error",
        ":indexeddb",
        ":transaction",
        "//closure/goog/asserts",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "error",
    srcs = ["error.js"],
    lenient = True,
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/debug:error",
    ],
)

closure_js_library(
    name = "index",
    srcs = ["index.js"],
    lenient = True,
    deps = [
        ":cursor",
        ":error",
        ":keyrange",
        "//closure/goog/debug",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "indexeddb",
    srcs = [
        "indexeddb.js",
        "transaction.js",
    ],
    lenient = True,
    deps = [
        ":error",
        ":objectstore",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "keyrange",
    srcs = ["keyrange.js"],
    lenient = True,
)

closure_js_library(
    name = "objectstore",
    srcs = ["objectstore.js"],
    lenient = True,
    deps = [
        ":cursor",
        ":error",
        ":index",
        ":keyrange",
        "//closure/goog/debug",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

alias(
    name = "transaction",
    actual = ":indexeddb",
)
