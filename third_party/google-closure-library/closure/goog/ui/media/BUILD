load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "mediamodel",
    srcs = ["mediamodel.js"],
    deps = [
        "//closure/goog/array",
        "//closure/goog/html:trustedresourceurl",
        "//closure/goog/math:size",
    ],
)

closure_js_library(
    name = "youtube",
    srcs = ["youtube.js"],
    deps = [
        ":mediamodel",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/html:trustedresourceurl",
        "//closure/goog/string",
        "//closure/goog/string:const",
        "//closure/goog/ui:component",
        "//closure/goog/ui:control",
    ],
)
