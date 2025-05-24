load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "window",
    srcs = ["window.js"],
    lenient = True,
    deps = [
        "//closure/goog/dom",
        "//closure/goog/dom:safe",
        "//closure/goog/dom:tagname",
        "//closure/goog/html:safeurl",
        "//closure/goog/html:uncheckedconversions",
        "//closure/goog/labs/useragent:platform",
        "//closure/goog/string",
        "//closure/goog/string:const",
        "//closure/goog/string:typedstring",
        "//closure/goog/useragent",
    ],
)
