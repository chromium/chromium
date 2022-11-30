load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "entries",
    srcs = ["entries.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/log",
        "//closure/goog/object",
    ],
)

closure_js_library(
    name = "registry",
    srcs = ["registry.js"],
    deps = [
        ":entries",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/log",
        "//closure/goog/string",
        "//closure/goog/uri:utils",
    ],
)

closure_js_library(
    name = "testhelpers",
    testonly = 1,
    srcs = ["testhelpers.js"],
    deps = [
        ":entries",
        ":registry",
        ":tweak",
    ],
)

closure_js_library(
    name = "tweak",
    srcs = ["tweak.js"],
    deps = [
        ":entries",
        ":registry",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "tweakui",
    srcs = ["tweakui.js"],
    deps = [
        ":entries",
        ":registry",
        ":tweak",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:safestylesheet",
        "//closure/goog/object",
        "//closure/goog/string:const",
        "//closure/goog/style",
        "//closure/goog/ui:zippy",
        "//closure/goog/useragent",
    ],
)
