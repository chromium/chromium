load("//third_party/bazel_rules/rules_cc/cc:objc_library.bzl", "objc_library")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLStringUtil",
    srcs = [
        "Sources/TFLStringUtil.mm",
    ],
    hdrs = [
        "Sources/TFLStringUtil.h",
    ],
    module_name = "TFLStringUtil",
)
