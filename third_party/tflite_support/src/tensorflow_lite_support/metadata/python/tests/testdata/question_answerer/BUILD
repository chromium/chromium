load("//tensorflow_lite_support/tools/build_rules:http_files.bzl", "tflite_model")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

filegroup(
    name = "test_files",
    srcs = glob([
        "*.json",
        "*.tflite",
        "*.txt",
    ]) + [":mobilebert_float"],
)

tflite_model(name = "mobilebert_float")
