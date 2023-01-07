load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "nondisposableeventtarget",
    srcs = ["nondisposableeventtarget.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/events:event",
        "//closure/goog/events:listenable",
        "//closure/goog/events:listenermap",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "touch",
    srcs = ["touch.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/events:eventtype",
        "//closure/goog/string",
    ],
)
