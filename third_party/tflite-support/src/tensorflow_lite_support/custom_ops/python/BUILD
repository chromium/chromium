package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "tflite_text_api",
    srcs = ["tflite_text_api.py"],
    deps = [
        "//tensorflow_lite_support/custom_ops:expect_tfpy_installed",
        "//tensorflow_lite_support/custom_ops:expect_tftext_installed",
    ],
)

py_library(
    name = "sentencepiece_tokenizer",
    srcs = ["sentencepiece_tokenizer.py"],
    data = [
        "//tensorflow_lite_support/custom_ops/kernel/sentencepiece:sentencepiece_detokenizer_op.so",
        "//tensorflow_lite_support/custom_ops/kernel/sentencepiece:sentencepiece_tokenizer_op.so",
    ],
    srcs_version = "PY3",
    deps = [
        "//tensorflow_lite_support/custom_ops:expect_tfpy_installed",
        "//tensorflow_lite_support/custom_ops/kernel/sentencepiece:pywrap_model_converter",
    ],
)

py_test(
    name = "sentencepiece_tokenizer_test",
    srcs = ["sentencepiece_tokenizer_test.py"],
    data = [
        "//tensorflow_lite_support/custom_ops/kernel/sentencepiece:testdata",
    ],
    python_version = "PY3",
    deps = [
        ":sentencepiece_tokenizer",
        "//tensorflow_lite_support/custom_ops:expect_tfpy_installed",
        "//tensorflow_lite_support/custom_ops:expect_tftext_installed",
        "//tensorflow_lite_support/custom_ops/kernel/sentencepiece:pywrap_tflite_registerer",
        "@absl_py//absl:app",
        "@absl_py//absl/flags",
        "@absl_py//absl/logging",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "ragged_tensor_to_tensor_test",
    srcs = ["ragged_tensor_to_tensor_test.py"],
    python_version = "PY3",
    deps = [
        "//tensorflow_lite_support/custom_ops:expect_tfpy_installed",
        "//tensorflow_lite_support/custom_ops/kernel/ragged:pywrap_tflite_registerer",
        "@absl_py//absl:app",
        "@absl_py//absl/flags",
        "@absl_py//absl/logging",
    ],
)
