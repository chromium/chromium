load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "cursor",
    srcs = ["cursor.js"],
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
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/debug:error",
    ],
)

closure_js_library(
    name = "index",
    srcs = ["index.js"],
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
    srcs = ["indexeddb.js"],
    deps = [
        ":error",
        ":objectstore",
        ":transaction",
        "//closure/goog/events:event",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
    ],
)

closure_js_library(
    name = "keyrange",
    srcs = ["keyrange.js"],
)

closure_js_library(
    name = "objectstore",
    srcs = ["objectstore.js"],
    deps = [
        ":cursor",
        ":error",
        ":index",
        ":keyrange",
        "//closure/goog/debug",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)

closure_js_library(
    name = "transaction",
    srcs = ["transaction.js"],
    deps = [
        ":error",
        ":objectstore",
        "//closure/goog/events",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtarget",
        "//third_party/closure/goog/mochikit/async:deferred",
    ],
)
