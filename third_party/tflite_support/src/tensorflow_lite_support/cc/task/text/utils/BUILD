load(
    "@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl",
    "cc_library_with_tflite",
)

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library_with_tflite(
    name = "universal_sentence_encoder_utils",
    srcs = ["universal_sentence_encoder_utils.cc"],
    hdrs = ["universal_sentence_encoder_utils.h"],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library_with_tflite(
    name = "bert_utils",
    srcs = ["bert_utils.cc"],
    hdrs = ["bert_utils.h"],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library_with_tflite(
    name = "text_op_resolver",
    srcs = ["text_op_resolver.cc"],
    hdrs = ["text_op_resolver.h"],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
    ],
    deps = [
        "//tensorflow_lite_support/custom_ops/kernel/ragged:ragged_tensor_to_tensor_tflite",  # fixdeps: keep
        "//tensorflow_lite_support/custom_ops/kernel/sentencepiece:sentencepiece_tokenizer_tflite",  # fixdeps: keep
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite/core/api:op_resolver",
    ],
)
