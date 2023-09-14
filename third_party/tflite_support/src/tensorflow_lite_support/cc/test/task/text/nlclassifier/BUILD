load(
    "@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl",
    "cc_test_with_tflite",
)

package(
    default_visibility = [
        "//visibility:private",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "nl_classifier_test_utils",
    srcs = ["nl_classifier_test_utils.cc"],
    hdrs = ["nl_classifier_test_utils.h"],
    visibility = ["//tensorflow_lite_support:internal"],
    deps = [
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "@org_tensorflow//tensorflow/lite:op_resolver",
        "@org_tensorflow//tensorflow/lite:string_util",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "@org_tensorflow//tensorflow/lite/kernels:kernel_util",
    ],
)

cc_test_with_tflite(
    name = "nl_classifier_test",
    srcs = ["nl_classifier_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:nl_classifier_models",
    ],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite:test_util",
        "//tensorflow_lite_support/cc/task/core:base_task_api",
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier",
    ],
    deps = [
        ":nl_classifier_test_utils",
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/test:test_utils",
        "@org_tensorflow//tensorflow/lite:string_util",
        "@org_tensorflow//tensorflow/lite/kernels:deprecated_backends",
        "@org_tensorflow//tensorflow/lite/kernels:kernel_util",
    ],
)
