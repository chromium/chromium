load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "buttonrenderer",
    srcs = ["buttonrenderer.js"],
    deps = [
        "//closure/goog/dom",
        "//closure/goog/dom:classlist",
        "//closure/goog/dom:tagname",
        "//closure/goog/ui:button",
        "//closure/goog/ui:controlcontent",
        "//closure/goog/ui:cssnames",
        "//closure/goog/ui:custombuttonrenderer",
        "//closure/goog/ui:registry",
    ],
)

closure_js_library(
    name = "menubuttonrenderer",
    srcs = ["menubuttonrenderer.js"],
    deps = [
        ":buttonrenderer",
        "//closure/goog/a11y/aria:roles",
        "//closure/goog/array",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/style",
        "//closure/goog/ui:control",
        "//closure/goog/ui:controlcontent",
        "//closure/goog/ui:menu",
        "//closure/goog/ui:menubutton",
        "//closure/goog/ui:menurenderer",
    ],
)

closure_js_library(
    name = "primaryactionbuttonrenderer",
    srcs = ["primaryactionbuttonrenderer.js"],
    deps = [
        ":buttonrenderer",
        "//closure/goog/ui:button",
        "//closure/goog/ui:registry",
    ],
)
