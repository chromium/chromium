# TensorFlow Lite Task Library

load(
    "@org_tensorflow//tensorflow/lite/ios:ios.bzl",
    "TFL_MINIMUM_OS_VERSION",
    "tflite_ios_framework",
)
load(
    "//tensorflow_lite_support/ios:ios.bzl",
    "strip_c_api_include_path_prefix",
)

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

strip_c_api_include_path_prefix(
    name = "strip_c_api_include_path",
    hdr_labels = [
        "//tensorflow_lite_support/c/task/text:bert_nl_classifier.h",
        "//tensorflow_lite_support/c/task/text:nl_classifier.h",
        "//tensorflow_lite_support/c/task/text:nl_classifier_common.h",
        "//tensorflow_lite_support/c/task/text:bert_question_answerer.h",
        "//tensorflow_lite_support/c/task/vision:image_classifier.h",
        "//tensorflow_lite_support/c/task/vision:object_detector.h",
        "//tensorflow_lite_support/c/task/vision:image_segmenter.h",
        "//tensorflow_lite_support/c/task/processor:bounding_box.h",
        "//tensorflow_lite_support/c/task/vision/core:frame_buffer.h",
        "//tensorflow_lite_support/c/task/processor:classification_result.h",
        "//tensorflow_lite_support/c/task/processor:detection_result.h",
        "//tensorflow_lite_support/c/task/processor:segmentation_result.h",
        "//tensorflow_lite_support/c/task/processor:classification_options.h",
        "//tensorflow_lite_support/c/task/processor:category.h",
        "//tensorflow_lite_support/c/task/core:base_options.h",
        "//tensorflow_lite_support/c:common.h",
    ],
)

# This target builds a monolithic static framework for the TFLite Text API,
# which includes the TFLite runtime in it.
#
# bazel build -c opt --config=ios_fat //tensorflow_lite_support/ios:TensorFlowLiteTaskTextC_framework
tflite_ios_framework(
    name = "TensorFlowLiteTaskTextC_framework",
    hdrs = [
        ":bert_nl_classifier.h",
        ":bert_question_answerer.h",
        ":nl_classifier.h",
        ":nl_classifier_common.h",
    ],
    allowlist_symbols_file = ":allowlist_TensorFlowLiteTaskText.txt",
    bundle_name = "TensorFlowLiteTaskTextC",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    deps = [
        "//tensorflow_lite_support/c/task/text:bert_nl_classifier",
        "//tensorflow_lite_support/c/task/text:bert_question_answerer",
        "//tensorflow_lite_support/c/task/text:nl_classifier",
    ],
)

# Xcode 12 does not support ios fat libraries. Frameworks built for multiple
# architectures should be compiled into a .xcframework inside. Bazel currently
# does not support building .xcframework. You have to build the framework
# for the architecture you decide to test on.
# Use the below command to build for arm64 which lets you test the library on
# iOS devices.
# bazel build -c opt --config=ios_arm64 //tensorflow_lite_support/ios:TensorFlowLiteTaskVisionC_framework
tflite_ios_framework(
    name = "TensorFlowLiteTaskVisionC_framework",
    hdrs = [
        ":base_options.h",
        ":bounding_box.h",
        ":category.h",
        ":classification_options.h",
        ":classification_result.h",
        ":common.h",
        ":detection_result.h",
        ":frame_buffer.h",
        ":image_classifier.h",
        ":image_segmenter.h",
        ":object_detector.h",
        ":segmentation_result.h",
    ],
    allowlist_symbols_file = ":allowlist_TensorFlowLiteTaskVision.txt",
    bundle_name = "TensorFlowLiteTaskVisionC",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    deps = [
        "//tensorflow_lite_support/c/task/vision:image_classifier",
        "//tensorflow_lite_support/c/task/vision:image_segmenter",
        "//tensorflow_lite_support/c/task/vision:object_detector",
    ],
)

objc_library(
    name = "TFLCommon",
    hdrs = [
        "sources/TFLCommon.h",
    ],
    module_name = "TFLCommon",
    visibility = [
        "//tensorflow_lite_support:__subpackages__",
    ],
)

objc_library(
    name = "TFLCommonUtils",
    srcs = [
        "sources/TFLCommonUtils.m",
    ],
    hdrs = [
        "sources/TFLCommonUtils.h",
    ],
    module_name = "TFLCommonUtils",
    visibility = [
        "//tensorflow_lite_support:__subpackages__",
    ],
    deps = [
        "//tensorflow_lite_support/c:common",
        "//tensorflow_lite_support/ios:TFLCommon",
    ],
)
