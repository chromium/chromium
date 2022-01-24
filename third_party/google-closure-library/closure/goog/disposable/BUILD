load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "disposable",
    srcs = [
        "disposable.js",
        "dispose.js",
        "disposeall.js",
    ],
    deps = [":idisposable"],
)

closure_js_library(
    name = "idisposable",
    srcs = ["idisposable.js"],
)
