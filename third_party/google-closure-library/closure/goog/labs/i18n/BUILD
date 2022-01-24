load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "listformat",
    srcs = ["listformat.js"],
    deps = [
        ":listsymbols",
        "//closure/goog/asserts",
    ],
)

closure_js_library(
    name = "listsymbols",
    srcs = ["listsymbols.js"],
)

closure_js_library(
    name = "listsymbolsext",
    srcs = ["listsymbolsext.js"],
    deps = [":listsymbols"],
)
