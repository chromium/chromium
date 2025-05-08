load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "abstractmodulemanager",
    srcs = ["abstractmodulemanager.js"],
    lenient = True,
    deps = [
        "//closure/goog/html:trustedresourceurl",
        "//closure/goog/module:abstractmoduleloader",
        "//closure/goog/module:basemodule",
        "//closure/goog/module:moduleinfo",
        "//closure/goog/module:moduleloadcallback",
    ],
)

closure_js_library(
    name = "activemodulemanager",
    srcs = ["activemodulemanager.js"],
    lenient = True,
    deps = [
        ":abstractmodulemanager",
        "//closure/goog/asserts",
    ],
)
