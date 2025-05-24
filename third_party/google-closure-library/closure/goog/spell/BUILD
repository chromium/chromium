load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "spellcheck",
    srcs = ["spellcheck.js"],
    lenient = True,
    deps = [
        "//closure/goog/events:event",
        "//closure/goog/events:eventtarget",
        "//closure/goog/structs:set",
        "//closure/goog/timer",
    ],
)
