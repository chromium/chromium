load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "data",
    srcs = ["data.js"],
    deps = [
        "//closure/goog/asserts",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:safescript",
        "//closure/goog/html:safestyle",
        "//closure/goog/html:safestylesheet",
        "//closure/goog/html:safeurl",
        "//closure/goog/html:trustedresourceurl",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/i18n:bidi",
        "//closure/goog/string:const",
        "//closure/goog/uri",
        "//javascript/typescript/safevalues:safevalues_js",
    ],
)

closure_js_library(
    name = "renderer",
    srcs = [
        "injecteddatasupplier.js",
        "renderer.js",
    ],
    deps = [
        ":data",
        ":soy",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/html:safehtml",
        "//closure/goog/html:safestylesheet",
    ],
)

closure_js_library(
    name = "soy",
    srcs = ["soy.js"],
    deps = [
        ":data",
        "//closure/goog/asserts",
        "//closure/goog/dom",
        "//closure/goog/dom:nodetype",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/html:safehtml",
    ],
)

closure_js_library(
    name = "soy_testhelper",
    testonly = 1,
    srcs = ["soy_testhelper.js"],
    deps = [
        ":data",
        "//closure/goog/dom",
        "//closure/goog/dom:tagname",
        "//closure/goog/i18n:bidi",
        "//closure/goog/string",
        "//closure/goog/testing:asserts",
        "//closure/goog/useragent",
    ],
)
