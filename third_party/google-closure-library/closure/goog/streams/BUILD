load("@io_bazel_rules_closure//closure:defs.bzl", "closure_js_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

closure_js_library(
    name = "defines",
    srcs = ["defines.js"],
)

closure_js_library(
    name = "full",
    srcs = ["full.js"],
    deps = [
        ":defines",
        ":full_impl",
        ":full_native_impl",
        ":full_types",
    ],
)

closure_js_library(
    name = "full_impl",
    srcs = ["full_impl.js"],
    deps = [
        ":full_types",
        ":lite_impl",
        "//closure/goog/asserts",
        "//closure/goog/promise:nativeresolver",
    ],
)

closure_js_library(
    name = "full_native_impl",
    srcs = ["full_native_impl.js"],
    deps = [
        ":full_types",
        ":lite_native_impl",
    ],
)

closure_js_library(
    name = "full_test_cases",
    testonly = 1,
    srcs = ["full_test_cases.js"],
    deps = [
        ":full_types",
        ":lite_test_cases",
        "//closure/goog/testing:recordfunction",
    ],
)

closure_js_library(
    name = "full_types",
    srcs = ["full_types.js"],
    deps = [":lite_types"],
)

closure_js_library(
    name = "lite",
    srcs = ["lite.js"],
    deps = [
        ":defines",
        ":lite_impl",
        ":lite_native_impl",
        ":lite_types",
    ],
)

closure_js_library(
    name = "lite_impl",
    srcs = ["lite_impl.js"],
    deps = [
        ":lite_types",
        "//closure/goog/asserts",
        "//closure/goog/promise:nativeresolver",
    ],
)

closure_js_library(
    name = "lite_native_impl",
    srcs = ["lite_native_impl.js"],
    deps = [":lite_types"],
)

closure_js_library(
    name = "lite_test_cases",
    testonly = 1,
    srcs = ["lite_test_cases.js"],
    deps = [
        ":lite_types",
        "//closure/goog/testing:jsunit",
    ],
)

closure_js_library(
    name = "lite_types",
    srcs = ["lite_types.js"],
)
