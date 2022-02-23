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
        # build rule placeholder: tensorflow dep,
        # build rule placeholder: tensorflow_text dep,
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
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/custom_ops/kernel/sentencepiece/py:pywrap_model_converter",
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
        # build rule placeholder: tensorflow dep,
        # build rule placeholder: tensorflow_text dep,
        "//tensorflow_lite_support/custom_ops/kernel/sentencepiece/py:pywrap_tflite_registerer",
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
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/custom_ops/kernel/ragged/py:pywrap_tflite_registerer",
        "@absl_py//absl:app",
        "@absl_py//absl/flags",
        "@absl_py//absl/logging",
    ],
)
