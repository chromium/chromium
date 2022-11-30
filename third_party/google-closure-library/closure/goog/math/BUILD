load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "affinetransform",
    srcs = ["affinetransform.js"],
)

closure_js_library(
    name = "bezier",
    srcs = ["bezier.js"],
    deps = [
        ":coordinate",
        ":math",
    ],
)

closure_js_library(
    name = "box",
    srcs = ["box.js"],
    deps = [
        ":coordinate",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "coordinate",
    srcs = ["coordinate.js"],
    deps = [":math"],
)

closure_js_library(
    name = "coordinate3",
    srcs = ["coordinate3.js"],
)

closure_js_library(
    name = "exponentialbackoff",
    srcs = ["exponentialbackoff.js"],
    deps = ["//closure/goog/asserts"],
)

closure_js_library(
    name = "integer",
    srcs = ["integer.js"],
    deps = ["//closure/goog/reflect"],
)

closure_js_library(
    name = "irect",
    srcs = ["irect.js"],
)

closure_js_library(
    name = "line",
    srcs = ["line.js"],
    deps = [
        ":coordinate",
        ":math",
    ],
)

closure_js_library(
    name = "long",
    srcs = ["long.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/reflect",
    ],
)

closure_js_library(
    name = "math",
    srcs = ["math.js"],
    deps = ["//closure/goog/asserts"],
)

closure_js_library(
    name = "matrix",
    srcs = ["matrix.js"],
    deps = [
        ":math",
        ":size",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/string",
    ],
)

closure_js_library(
    name = "path",
    srcs = ["path.js"],
    deps = [
        ":affinetransform",
        ":math",
        "//closure/goog/array",
    ],
)

closure_js_library(
    name = "paths",
    srcs = ["paths.js"],
    deps = [
        ":coordinate",
        ":path",
    ],
)

closure_js_library(
    name = "range",
    srcs = ["range.js"],
    deps = ["//closure/goog/asserts"],
)

closure_js_library(
    name = "rangeset",
    srcs = ["rangeset.js"],
    deps = [
        ":range",
        "//closure/goog/array",
        "//closure/goog/iter",
    ],
)

closure_js_library(
    name = "rect",
    srcs = ["rect.js"],
    deps = [
        ":box",
        ":coordinate",
        ":irect",
        ":size",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "size",
    srcs = ["size.js"],
)

closure_js_library(
    name = "tdma",
    srcs = ["tdma.js"],
)

closure_js_library(
    name = "vec2",
    srcs = ["vec2.js"],
    deps = [
        ":coordinate",
        ":math",
    ],
)

closure_js_library(
    name = "vec3",
    srcs = ["vec3.js"],
    deps = [
        ":coordinate3",
        ":math",
    ],
)
