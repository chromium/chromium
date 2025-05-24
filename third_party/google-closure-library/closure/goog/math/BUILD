load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "affinetransform",
    srcs = ["affinetransform.js"],
    lenient = True,
)

closure_js_library(
    name = "bezier",
    srcs = ["bezier.js"],
    lenient = True,
    deps = [
        ":coordinate",
        ":math",
    ],
)

closure_js_library(
    name = "box",
    srcs = ["box.js"],
    lenient = True,
    deps = [
        ":coordinate",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "coordinate",
    srcs = ["coordinate.js"],
    lenient = True,
    deps = [":math"],
)

closure_js_library(
    name = "coordinate3",
    srcs = ["coordinate3.js"],
    lenient = True,
)

closure_js_library(
    name = "exponentialbackoff",
    srcs = ["exponentialbackoff.js"],
    lenient = True,
    deps = ["//closure/goog/asserts"],
)

closure_js_library(
    name = "integer",
    srcs = ["integer.js"],
    lenient = True,
    deps = ["//closure/goog/reflect"],
)

closure_js_library(
    name = "irect",
    srcs = ["irect.js"],
    lenient = True,
)

closure_js_library(
    name = "line",
    srcs = ["line.js"],
    lenient = True,
    deps = [
        ":coordinate",
        ":math",
    ],
)

closure_js_library(
    name = "long",
    srcs = ["long.js"],
    lenient = True,
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/reflect",
    ],
)

closure_js_library(
    name = "math",
    srcs = ["math.js"],
    lenient = True,
    deps = ["//closure/goog/asserts"],
)

closure_js_library(
    name = "matrix",
    srcs = ["matrix.js"],
    lenient = True,
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
    lenient = True,
    deps = [
        ":affinetransform",
        ":math",
        "//closure/goog/array",
    ],
)

closure_js_library(
    name = "paths",
    srcs = ["paths.js"],
    lenient = True,
    deps = [
        ":coordinate",
        ":path",
    ],
)

closure_js_library(
    name = "range",
    srcs = ["range.js"],
    lenient = True,
    deps = ["//closure/goog/asserts"],
)

closure_js_library(
    name = "rangeset",
    srcs = ["rangeset.js"],
    lenient = True,
    deps = [
        ":range",
        "//closure/goog/array",
        "//closure/goog/iter",
    ],
)

closure_js_library(
    name = "rect",
    srcs = ["rect.js"],
    lenient = True,
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
    lenient = True,
)

closure_js_library(
    name = "tdma",
    srcs = ["tdma.js"],
    lenient = True,
)

closure_js_library(
    name = "vec2",
    srcs = ["vec2.js"],
    lenient = True,
    deps = [
        ":coordinate",
        ":math",
    ],
)

closure_js_library(
    name = "vec3",
    srcs = ["vec3.js"],
    lenient = True,
    deps = [
        ":coordinate3",
        ":math",
    ],
)
