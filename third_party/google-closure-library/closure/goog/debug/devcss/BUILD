load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "devcss",
    srcs = ["devcss.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/cssom",
        "//closure/goog/dom:classlist",
        "//closure/goog/events",
        "//closure/goog/events:eventtype",
        "//closure/goog/string",
        "//closure/goog/useragent",
    ],
)

closure_js_library(
    name = "devcssrunner",
    srcs = ["devcssrunner.js"],
    deps = [":devcss"],
)
