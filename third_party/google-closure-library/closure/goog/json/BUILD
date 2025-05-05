load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "hybrid",
    srcs = ["hybrid.js"],
    lenient = True,
    deps = [
        ":json",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "json",
    srcs = ["json.js"],
    lenient = True,
)

closure_js_library(
    name = "jsonable",
    srcs = ["jsonable.js"],
    lenient = True,
)

closure_js_library(
    name = "nativejsonprocessor",
    srcs = ["nativejsonprocessor.js"],
    lenient = True,
    deps = [
        ":types",
        "//closure/goog/asserts",
        "//closure/goog/string:parser",
        "//closure/goog/string:stringifier",
    ],
)

closure_js_library(
    name = "types",
    srcs = ["types.js"],
    lenient = True,
)
