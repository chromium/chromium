load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite")

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library_with_tflite(
    name = "image_classifier",
    srcs = [
        "image_classifier.cc",
    ],
    hdrs = [
        "image_classifier.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/vision:image_classifier",
    ],
    deps = [
        "//tensorflow_lite_support/c:common",
        "//tensorflow_lite_support/c:common_utils",
        "//tensorflow_lite_support/c/task/core:base_options",
        "//tensorflow_lite_support/c/task/core/utils:base_options_utils",
        "//tensorflow_lite_support/c/task/processor:bounding_box",
        "//tensorflow_lite_support/c/task/processor:classification_options",
        "//tensorflow_lite_support/c/task/processor:classification_result",
        "//tensorflow_lite_support/c/task/processor/utils:classification_options_utils",
        "//tensorflow_lite_support/c/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/c/task/vision/utils:frame_buffer_cpp_c_utils",
        "//tensorflow_lite_support/cc/task/vision/proto:classifications_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_classifier_options_proto_inc",
    ],
)

cc_library_with_tflite(
    name = "object_detector",
    srcs = [
        "object_detector.cc",
    ],
    hdrs = [
        "object_detector.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/vision:object_detector",
    ],
    deps = [
        "//tensorflow_lite_support/c:common",
        "//tensorflow_lite_support/c:common_utils",
        "//tensorflow_lite_support/c/task/core:base_options",
        "//tensorflow_lite_support/c/task/core/utils:base_options_utils",
        "//tensorflow_lite_support/c/task/processor:bounding_box",
        "//tensorflow_lite_support/c/task/processor:classification_options",
        "//tensorflow_lite_support/c/task/processor:detection_result",
        "//tensorflow_lite_support/c/task/processor/utils:classification_options_utils",
        "//tensorflow_lite_support/c/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/c/task/vision/utils:frame_buffer_cpp_c_utils",
        "//tensorflow_lite_support/cc/task/vision/proto:detections_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:object_detector_options_proto_inc",
    ],
)

cc_library_with_tflite(
    name = "image_segmenter",
    srcs = [
        "image_segmenter.cc",
    ],
    hdrs = [
        "image_segmenter.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/vision:image_segmenter",
    ],
    deps = [
        "//tensorflow_lite_support/c:common",
        "//tensorflow_lite_support/c:common_utils",
        "//tensorflow_lite_support/c/task/core:base_options",
        "//tensorflow_lite_support/c/task/core/utils:base_options_utils",
        "//tensorflow_lite_support/c/task/processor:segmentation_result",
        "//tensorflow_lite_support/c/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/c/task/vision/utils:frame_buffer_cpp_c_utils",
        "//tensorflow_lite_support/cc/task/vision/proto:image_segmenter_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:segmentations_proto_inc",
    ],
)
