load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "sets",
    srcs = ["sets.js"],
    lenient = True,
    deps = ["//closure/goog/collections:iters"],
)

closure_js_library(
    name = "iters",
    srcs = ["iters.js"],
    lenient = True,
)

closure_js_library(
    name = "maps",
    srcs = ["maps.js"],
    lenient = True,
)
