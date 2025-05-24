load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "countries",
    srcs = ["countries.js"],
    lenient = True,
)

closure_js_library(
    name = "defaultlocalenameconstants",
    srcs = ["defaultlocalenameconstants.js"],
    lenient = True,
)

closure_js_library(
    name = "locale",
    srcs = ["locale.js"],
    lenient = True,
    deps = [":nativenameconstants"],
)

closure_js_library(
    name = "nativenameconstants",
    srcs = ["nativenameconstants.js"],
    lenient = True,
)

closure_js_library(
    name = "scriptToLanguages",
    srcs = ["scriptToLanguages.js"],
    lenient = True,
    deps = [":locale"],
)

closure_js_library(
    name = "timezonedetection",
    srcs = ["timezonedetection.js"],
    lenient = True,
    deps = [
        ":timezonefingerprint",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "timezonefingerprint",
    srcs = ["timezonefingerprint.js"],
    lenient = True,
)

closure_js_library(
    name = "timezonelist",
    srcs = ["timezonelist.js"],
    lenient = True,
    deps = [":locale"],
)
