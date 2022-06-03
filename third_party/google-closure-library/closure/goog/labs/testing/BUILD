load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "assertthat",
    srcs = [
        "assertthat.js",
        "matchererror.js",
    ],
    deps = [
        ":matcher",
        "//closure/goog/debug:error",
    ],
)

alias(
    name = "decoratormatcher",
    actual = ":matchers",
)

alias(
    name = "dictionarymatcher",
    actual = ":matchers",
)

closure_js_library(
    name = "environment",
    srcs = ["environment.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/debug:console",
        "//closure/goog/promise:thenable",
        "//closure/goog/testing:jsunit",
        "//closure/goog/testing:mockclock",
        "//closure/goog/testing:mockcontrol",
        "//closure/goog/testing:propertyreplacer",
        "//closure/goog/testing:testcase",
    ],
)

closure_js_library(
    name = "json_fuzzing",
    srcs = ["json_fuzzing.js"],
    deps = [
        "//closure/goog/string",
        "//closure/goog/testing:pseudorandom",
    ],
)

alias(
    name = "logicmatcher",
    actual = ":matchers",
)

closure_js_library(
    name = "matcher",
    srcs = ["matcher.js"],
)

closure_js_library(
    name = "matchers",
    srcs = [
        "decoratormatcher.js",
        "dictionarymatcher.js",
        "logicmatcher.js",
        "matchers.js",
        "numbermatcher.js",
        "objectmatcher.js",
        "stringmatcher.js",
    ],
    deps = [
        ":matcher",
        "//closure/goog/array",
        "//closure/goog/asserts",
        "//closure/goog/object",
        "//closure/goog/string",
    ],
)

alias(
    name = "numbermatcher",
    actual = ":matchers",
)

alias(
    name = "objectmatcher",
    actual = ":matchers",
)

alias(
    name = "stringmatcher",
    actual = ":matchers",
)
