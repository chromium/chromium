load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "dom",
    testonly = True,
    srcs = ["dom.js"],
    lenient = True,
    deps = [
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:tagiterator",
        "//closure/goog/iter",
        "//closure/goog/string",
        "//closure/goog/testing:asserts",
    ],
)

closure_js_library(
    name = "fieldmock",
    testonly = True,
    srcs = ["fieldmock.js"],
    lenient = True,
    deps = [
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:range",
        "//closure/goog/editor:field",
        "//closure/goog/testing:loosemock",
        "//closure/goog/testing:mockmatchers",
    ],
)

closure_js_library(
    name = "testhelper",
    testonly = True,
    srcs = ["testhelper.js"],
    lenient = True,
    deps = [
        "//closure/goog/disposable",
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:range",
        "//closure/goog/editor:browserfeature",
        "//closure/goog/editor:node",
        "//closure/goog/editor/plugins:abstractbubbleplugin",
        "//closure/goog/testing:dom",
    ],
)
