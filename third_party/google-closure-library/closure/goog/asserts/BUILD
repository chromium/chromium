load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "asserts",
    srcs = ["asserts.js"],
    lenient = True,
    deps = [
        "//closure/goog/debug:error",
        "//closure/goog/dom:nodetype",
    ],
)

closure_js_library(
    name = "dom",
    srcs = ["dom.js"],
    lenient = True,
    deps = [
        ":asserts",
        "//closure/goog/dom:element",
        "//closure/goog/dom:tagname",
    ],
)
