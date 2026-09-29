load("//third_party/bazel_rules/rules_cc/cc:cc_library.bzl", "cc_library")

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "classification_options_utils",
    srcs = ["classification_options_utils.cc"],
    hdrs = ["classification_options_utils.h"],
    deps = [
        "//tensorflow_lite_support/c/task/processor:classification_options",
    ],
)
