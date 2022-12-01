load("//tensorflow_lite_support/tools/build_rules:http_files.bzl", "tflite_file", "tflite_model")

package(
    default_applicable_licenses = ["//tensorflow_lite_support:license"],
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

filegroup(
    name = "test_files",
    srcs = glob([
        "*.json",
        "*.tflite",
        "*.txt",
        "*.model",
    ]) + [
        ":bert_nl_classifier_no_metadata",
        "mobilebert_vocab",
        ":30k-clean",
    ],
)

tflite_model(name = "bert_nl_classifier_no_metadata")

tflite_file(
    name = "mobilebert_vocab",
    extension = "txt",
)

tflite_file(
    name = "30k-clean",
    extension = "model",
)
