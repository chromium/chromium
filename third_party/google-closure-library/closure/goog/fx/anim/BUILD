load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "anim",
    srcs = ["anim.js"],
    deps = [
        "//closure/goog/async:animationdelay",
        "//closure/goog/async:delay",
        "//closure/goog/disposable",
        "//closure/goog/object",
    ],
)
