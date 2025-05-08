load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "const",
    srcs = ["const.js"],
    lenient = True,
    deps = [
        ":typedstring",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "internal",
    srcs = ["internal.js"],
    lenient = True,
)

closure_js_library(
    name = "linkify",
    srcs = ["linkify.js"],
    lenient = True,
    deps = [
        ":const",
        ":string",
        "//closure/goog/asserts",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:uncheckedconversions",
    ],
)

closure_js_library(
    name = "newlines",
    srcs = ["newlines.js"],
    lenient = True,
    deps = ["//closure/goog/array"],
)

closure_js_library(
    name = "parser",
    srcs = ["parser.js"],
    lenient = True,
)

closure_js_library(
    name = "path",
    srcs = ["path.js"],
    lenient = True,
    deps = [
        ":string",
        "//closure/goog/array",
    ],
)

closure_js_library(
    name = "string",
    srcs = ["string.js"],
    lenient = True,
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
    lenient = True,
)

closure_js_library(
    name = "stringformat",
    srcs = ["stringformat.js"],
    lenient = True,
    deps = [":string"],
)

closure_js_library(
    name = "stringifier",
    srcs = ["stringifier.js"],
    lenient = True,
)

closure_js_library(
    name = "typedstring",
    srcs = ["typedstring.js"],
    lenient = True,
)
