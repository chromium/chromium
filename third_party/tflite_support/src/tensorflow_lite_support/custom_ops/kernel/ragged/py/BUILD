# Python wrapper used for test.

load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")

package(
    default_applicable_licenses = ["//tensorflow_lite_support:license"],
    default_visibility = [
        "//tensorflow_lite_support:users",
    ],
    licenses = ["notice"],  # Apache 2.0
)

pybind_extension(
    name = "pywrap_tflite_registerer",
    srcs = [
        "pywrap_tflite_registerer.cc",
    ],
    additional_exported_symbols = ["TFLite_RaggedTensorToTensorRegisterer"],
    module_name = "pywrap_tflite_registerer",
    srcs_version = "PY3ONLY",
    deps = [
        "//tensorflow_lite_support/custom_ops/kernel/ragged:py_tflite_registerer",
        "@local_config_python//:python_headers",
        "@org_tensorflow//tensorflow/lite:framework",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "@pybind11",
    ],
)
