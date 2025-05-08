load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "coordinates",
    srcs = ["coordinates.js"],
    lenient = True,
    deps = ["//closure/goog/string"],
)

alias(
    name = "element",
    actual = ":graphics",
)

closure_js_library(
    name = "ellipse",
    srcs = ["ellipse.js"],
    lenient = True,
    deps = [
        ":group",
        ":strokeandfillelement",
    ],
)

closure_js_library(
    name = "ext",
    srcs = ["ext.js"],
    lenient = True,
    deps = [
        ":coordinates",
        ":ellipse",
        ":graphics",
        ":image",
        ":rectangle",
        ":shape",
    ],
)

closure_js_library(
    name = "graphics",
    srcs = [
        "element.js",
        "graphics.js",
        "group.js",
    ],
    lenient = True,
    deps = [
        ":coordinates",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/events",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:eventtype",
        "//closure/goog/functions",
        "//closure/goog/graphics",
        "//closure/goog/graphics:abstractgraphics",
        "//closure/goog/math:coordinate",
        "//closure/goog/math:size",
    ],
)

alias(
    name = "group",
    actual = ":graphics",
)

closure_js_library(
    name = "image",
    srcs = ["image.js"],
    lenient = True,
    deps = [":element"],
)

closure_js_library(
    name = "path",
    srcs = ["path.js"],
    lenient = True,
    deps = [
        "//closure/goog/graphics:affinetransform",
        "//closure/goog/graphics:path",
        "//closure/goog/math:rect",
    ],
)

closure_js_library(
    name = "rectangle",
    srcs = ["rectangle.js"],
    lenient = True,
    deps = [
        ":group",
        ":strokeandfillelement",
    ],
)

closure_js_library(
    name = "shape",
    srcs = ["shape.js"],
    lenient = True,
    deps = [
        ":group",
        ":path",
        ":strokeandfillelement",
        "//closure/goog/graphics:abstractgraphics",
        "//closure/goog/math:rect",
    ],
)

closure_js_library(
    name = "strokeandfillelement",
    srcs = ["strokeandfillelement.js"],
    lenient = True,
    deps = [
        ":element",
        ":group",
        "//closure/goog/graphics:abstractgraphics",
    ],
)
