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

# To test it with Bazel, run the following command from the terminal of your desktop:
# bazel test tensorflow_lite_support/c/test/task/vision:image_classifier_test
cc_test_with_tflite(
    name = "image_classifier_test",
    srcs = ["image_classifier_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/c/task/vision:image_classifier",
        "@org_tensorflow//tensorflow/lite:test_util",
    ],
    deps = [
        "//tensorflow_lite_support/c:common",
        "//tensorflow_lite_support/c/task/processor:classification_result",
        "//tensorflow_lite_support/c/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "//tensorflow_lite_support/cc/test:test_utils",
    ],
)

# bazel test tensorflow_lite_support/c/test/task/vision:object_detector_test
cc_test_with_tflite(
    name = "object_detector_test",
    srcs = ["object_detector_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/c/task/vision:object_detector",
        "@org_tensorflow//tensorflow/lite:test_util",
    ],
    deps = [
        "//tensorflow_lite_support/c:common",
        "//tensorflow_lite_support/c/task/processor:detection_result",
        "//tensorflow_lite_support/c/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "//tensorflow_lite_support/cc/test:test_utils",
    ],
)

cc_test_with_tflite(
    name = "image_segmenter_test",
    srcs = ["image_segmenter_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/c/task/vision:image_segmenter",
        "@org_tensorflow//tensorflow/lite:test_util",
    ],
    deps = [
        "//tensorflow_lite_support/c:common",
        "//tensorflow_lite_support/c/task/processor:segmentation_result",
        "//tensorflow_lite_support/c/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "//tensorflow_lite_support/cc/test:test_utils",
    ],
)
