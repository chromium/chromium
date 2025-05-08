load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "abstractrange",
    srcs = ["abstractrange.js"],
    lenient = True,
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:abstractrange",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:rangeendpoint",
        "//closure/goog/dom:tagname",
        "//closure/goog/dom:textrangeiterator",
        "//closure/goog/iter",
        "//closure/goog/math:coordinate",
        "//closure/goog/string",
        "//closure/goog/string:stringbuffer",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "browserrange",
    srcs = ["browserrange.js"],
    lenient = True,
    deps = [
        ":abstractrange",
        ":geckorange",
        ":w3crange",
        ":webkitrange",
        "//closure/goog/dom",
        "//closure/goog/dom:browserfeature",
        "//closure/goog/dom:nodetype",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "geckorange",
    srcs = ["geckorange.js"],
    lenient = True,
    deps = [":w3crange"],
)

closure_js_library(
    name = "w3crange",
    srcs = ["w3crange.js"],
    lenient = True,
    deps = [
        ":abstractrange",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:rangeendpoint",
        "//closure/goog/dom:tagname",
        "//closure/goog/string",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "webkitrange",
    srcs = ["webkitrange.js"],
    lenient = True,
    deps = [
        ":w3crange",
        "//closure/goog/dom:rangeendpoint",
        "//closure/goog/useragent",
    ],
)
