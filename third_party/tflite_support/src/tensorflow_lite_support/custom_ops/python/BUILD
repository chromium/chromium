# Placeholder: load py_library
# Placeholder: load py_test

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
