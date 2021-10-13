load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "countries",
    srcs = ["countries.js"],
)

closure_js_library(
    name = "defaultlocalenameconstants",
    srcs = ["defaultlocalenameconstants.js"],
)

closure_js_library(
    name = "locale",
    srcs = ["locale.js"],
    deps = [":nativenameconstants"],
)

closure_js_library(
    name = "nativenameconstants",
    srcs = ["nativenameconstants.js"],
)

closure_js_library(
    name = "scriptToLanguages",
    srcs = ["scriptToLanguages.js"],
    deps = [":locale"],
)

closure_js_library(
    name = "timezonedetection",
    srcs = ["timezonedetection.js"],
    deps = [
        ":timezonefingerprint",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "timezonefingerprint",
    srcs = ["timezonefingerprint.js"],
)

closure_js_library(
    name = "timezonelist",
    srcs = ["timezonelist.js"],
    deps = [":locale"],
)
