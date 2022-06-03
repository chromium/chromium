load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "bidi",
    srcs = ["bidi.js"],
    deps = [
        ":style",
        "//closure/goog/dom",
        "//closure/goog/useragent",
        "//closure/goog/useragent:platform",
        "//closure/goog/useragent:product",
        "//closure/goog/useragent:product_isversion",
    ],
)

closure_js_library(
    name = "cursor",
    srcs = ["cursor.js"],
    deps = ["//closure/goog/useragent"],
)

closure_js_library(
    name = "style",
    srcs = ["style.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/dom:vendor",
        "//closure/goog/events:event",
        "//closure/goog/html:safestylesheet",
        "//closure/goog/math:box",
        "//closure/goog/math:coordinate",
        "//closure/goog/math:rect",
        "//closure/goog/math:size",
        "//closure/goog/object",
        "//closure/goog/reflect",
        "//closure/goog/string",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "stylescrollbartester",
    testonly = 1,
    srcs = ["stylescrollbartester.js"],
    deps = [
        ":style",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/testing:asserts",
    ],
)

closure_js_library(
    name = "transform",
    srcs = ["transform.js"],
    deps = [
        ":style",
        "//closure/goog/functions",
        "//closure/goog/math:coordinate",
        "//closure/goog/math:coordinate3",
        "//closure/goog/useragent",
        "//closure/goog/useragent:product_isversion",
    ],
)

closure_js_library(
    name = "transition",
    srcs = ["transition.js"],
    deps = [
        ":style",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/dom:vendor",
        "//closure/goog/functions",
        "//closure/goog/html:safehtml",
        "//closure/goog/useragent",
    ],
)
