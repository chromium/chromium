load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "httpcors",
    srcs = ["httpcors.js"],
    lenient = True,
    deps = [
        "//closure/goog/object",
        "//closure/goog/string",
        "//closure/goog/uri",
        "//closure/goog/uri:utils",
    ],
)
