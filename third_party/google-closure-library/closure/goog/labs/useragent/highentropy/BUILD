load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

closure_js_library(
    name = "highentropydata",
    srcs = ["highentropydata.js"],
    lenient = True,
    deps = [":highentropyvalue"],
)

closure_js_library(
    name = "highentropyvalue",
    srcs = ["highentropyvalue.js"],
    lenient = True,
    deps = [
        "//closure/goog/labs/useragent:util",
        "//closure/goog/string:internal",
    ],
)
