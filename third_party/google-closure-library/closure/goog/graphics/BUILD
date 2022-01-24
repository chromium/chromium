load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "abstractgraphics",
    srcs = [
        "abstractgraphics.js",
        "affinetransform.js",
        "element.js",
        "ellipseelement.js",
        "fill.js",
        "font.js",
        "groupelement.js",
        "imageelement.js",
        "path.js",
        "pathelement.js",
        "rectelement.js",
        "solidfill.js",
        "stroke.js",
        "strokeandfillelement.js",
        "textelement.js",
    ],
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/events",
        "//closure/goog/events:eventtarget",
        "//closure/goog/events:listenable",
        "//closure/goog/math",
        "//closure/goog/math:coordinate",
        "//closure/goog/math:size",
        "//closure/goog/style",
        "//closure/goog/ui:component",
    ],
)

alias(
    name = "affinetransform",
    actual = ":abstractgraphics",
)

alias(
    name = "canvaselement",
    actual = ":canvasgraphics",
)

closure_js_library(
    name = "canvasgraphics",
    srcs = [
        "canvaselement.js",
        "canvasgraphics.js",
    ],
    deps = [
        ":abstractgraphics",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/events:eventtype",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/math",
        "//closure/goog/math:size",
        "//closure/goog/string",
        "//closure/goog/string:const",
        "//closure/goog/style",
    ],
)

alias(
    name = "element",
    actual = ":abstractgraphics",
)

alias(
    name = "ellipseelement",
    actual = ":abstractgraphics",
)

alias(
    name = "fill",
    actual = ":abstractgraphics",
)

alias(
    name = "font",
    actual = ":abstractgraphics",
)

closure_js_library(
    name = "graphics",
    srcs = [
        "graphics.js",
    ],
    deps = [
        ":abstractgraphics",
        ":canvasgraphics",
        ":svggraphics",
        ":vmlgraphics",
        "//closure/goog/dom",
        "//closure/goog/useragent",
    ],
)

alias(
    name = "groupelement",
    actual = ":abstractgraphics",
)

alias(
    name = "imageelement",
    actual = ":abstractgraphics",
)

closure_js_library(
    name = "lineargradient",
    srcs = ["lineargradient.js"],
    deps = [
        ":abstractgraphics",
        "//closure/goog/asserts",
    ],
)

alias(
    name = "path",
    actual = ":abstractgraphics",
)

alias(
    name = "pathelement",
    actual = ":abstractgraphics",
)

closure_js_library(
    name = "paths",
    srcs = ["paths.js"],
    deps = [
        ":abstractgraphics",
        "//closure/goog/math:coordinate",
    ],
)

alias(
    name = "rectelement",
    actual = ":abstractgraphics",
)

alias(
    name = "solidfill",
    actual = ":abstractgraphics",
)

alias(
    name = "stroke",
    actual = ":abstractgraphics",
)

alias(
    name = "strokeandfillelement",
    actual = ":abstractgraphics",
)

alias(
    name = "svgelement",
    actual = ":svggraphics",
)

closure_js_library(
    name = "svggraphics",
    srcs = [
        "svgelement.js",
        "svggraphics.js",
    ],
    deps = [
        ":abstractgraphics",
        ":lineargradient",
        "//closure/goog/dom",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/math",
        "//closure/goog/math:size",
        "//closure/goog/style",
        "//closure/goog/timer",
        "//closure/goog/useragent",
    ],
)

alias(
    name = "textelement",
    actual = ":graphics",
)

alias(
    name = "vmlelement",
    actual = ":graphics",
)

closure_js_library(
    name = "vmlgraphics",
    srcs = [
        "vmlelement.js",
        "vmlgraphics.js",
    ],
    deps = [
        ":abstractgraphics",
        ":lineargradient",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/events",
        "//closure/goog/events:browserevent",
        "//closure/goog/events:eventhandler",
        "//closure/goog/events:eventtype",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/math",
        "//closure/goog/math:size",
        "//closure/goog/reflect",
        "//closure/goog/string",
        "//closure/goog/string:const",
        "//closure/goog/style",
        "//closure/goog/useragent",
    ],
)
