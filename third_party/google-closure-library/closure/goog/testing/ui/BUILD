load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "rendererasserts",
    srcs = ["rendererasserts.js"],
    deps = [
        "//closure/goog/testing:asserts",
        "//closure/goog/ui:controlrenderer",
    ],
)

closure_js_library(
    name = "rendererharness",
    srcs = ["rendererharness.js"],
    deps = [
        "//closure/goog/disposable",
        "//closure/goog/dom:nodetype",
        "//closure/goog/testing:asserts",
        "//closure/goog/testing:dom",
        "//closure/goog/ui:control",
        "//closure/goog/ui:controlrenderer",
    ],
)

closure_js_library(
    name = "style",
    srcs = ["style.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/testing:asserts",
    ],
)
