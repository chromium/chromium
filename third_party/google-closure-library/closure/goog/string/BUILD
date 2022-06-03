load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "const",
    srcs = ["const.js"],
    deps = [
        ":typedstring",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "internal",
    srcs = ["internal.js"],
)

closure_js_library(
    name = "linkify",
    srcs = ["linkify.js"],
    deps = [
        ":string",
        "//closure/goog/html:safehtml",
    ],
)

closure_js_library(
    name = "newlines",
    srcs = ["newlines.js"],
    deps = ["//closure/goog/array"],
)

closure_js_library(
    name = "parser",
    srcs = ["parser.js"],
)

closure_js_library(
    name = "path",
    srcs = ["path.js"],
    deps = [
        ":string",
        "//closure/goog/array",
    ],
)

closure_js_library(
    name = "string",
    srcs = ["string.js"],
    deps = [
        ":const",
        ":internal",
        "//closure/goog/dom:safe",
        "//closure/goog/html:uncheckedconversions",
    ],
)

closure_js_library(
    name = "stringbuffer",
    srcs = ["stringbuffer.js"],
)

closure_js_library(
    name = "stringformat",
    srcs = ["stringformat.js"],
    deps = [":string"],
)

closure_js_library(
    name = "stringifier",
    srcs = ["stringifier.js"],
)

closure_js_library(
    name = "typedstring",
    srcs = ["typedstring.js"],
)
