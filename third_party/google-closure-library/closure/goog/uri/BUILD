load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "uri",
    srcs = ["uri.js"],
    lenient = True,
    deps = [
        ":utils",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/collections:maps",
        "//closure/goog/string",
        "//closure/goog/structs",
    ],
)

closure_js_library(
    name = "utils",
    srcs = ["utils.js"],
    lenient = True,
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/string",
    ],
)
