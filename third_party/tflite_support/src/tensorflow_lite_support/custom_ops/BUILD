load("@org_tensorflow//tensorflow/lite/delegates/flex:build_def.bzl", "tflite_flex_cc_library")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

# This will generate the tf_text_flex_delegate cc_library, which is a custom
# flex delegate that only contains ops in listed models.
tflite_flex_cc_library(
    name = "tf_text_flex_delegate",
    additional_deps = ["@org_tensorflow_text//tensorflow_text:ops_lib"],
    models = [
        # TODO(b/160817619) Replace with a more complex model.
        "testdata/sentencepiece_tokenizer_flex_op.tflite",
    ],
)

# bazel test --config=monolithic tensorflow_lite_support/custom_ops:tflite_inference_test
cc_test(
    name = "tflite_inference_test",
    srcs = ["tflite_inference_main.cc"],
    args = ["--model=tensorflow_lite_support/custom_ops/testdata/sentencepiece_tokenizer_flex_op.tflite"],
    data = ["//tensorflow_lite_support/custom_ops:testdata/sentencepiece_tokenizer_flex_op.tflite"],
    deps = [
        ":tf_text_flex_delegate",
        "@org_tensorflow//tensorflow/lite:framework",
        "@org_tensorflow//tensorflow/lite:string_util",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "@org_tensorflow//tensorflow/lite/tools:command_line_flags",
    ] + select({
        "@org_tensorflow//tensorflow:android": [
            "@org_tensorflow//tensorflow/core:portable_tensorflow_lib_lite",
        ],
        "@org_tensorflow//tensorflow:ios": [
            "@org_tensorflow//tensorflow/core:portable_tensorflow_lib_lite",
        ],
        "//conditions:default": [
            "@org_tensorflow//tensorflow/core:lib",
        ],
    }),
)

# We expect several libraries to already be installed on the system, e.g. via
# `pip install numpy`
py_library(
    name = "expect_numpy_installed",
)

# `pip install tensorflow`
py_library(
    name = "expect_tfpy_installed",
)

# `pip install tensorflow_text`
py_library(
    name = "expect_tftext_installed",
)
