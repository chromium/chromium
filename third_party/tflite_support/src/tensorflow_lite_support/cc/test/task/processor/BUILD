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

cc_test_with_tflite(
    name = "image_preprocessor_test",
    srcs = ["image_preprocessor_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/processor:image_preprocessor",
        "@org_tensorflow//tensorflow/lite/core/shims:cc_shims_test_util",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/cc/test:test_utils",
        "//tensorflow_lite_support/examples/task/vision/desktop/utils:image_utils",
        "@com_google_absl//absl/status",
    ],
)
