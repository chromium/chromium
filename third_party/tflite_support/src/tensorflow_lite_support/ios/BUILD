# TensorFlow Lite Task Library

load(
    "@org_tensorflow//tensorflow/lite/ios:ios.bzl",
    "TFL_MINIMUM_OS_VERSION",
    "tflite_ios_framework",
)
load(
    "//tensorflow_lite_support/ios:ios.bzl",
    "TFL_TASK_MINIMUM_OS_VERSION",
    "strip_api_include_path_prefix",
)
load(
    "@build_bazel_rules_apple//apple:ios.bzl",
    "ios_static_framework",
)

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

strip_api_include_path_prefix(
    name = "strip_api_include_path",
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
        "//tensorflow_lite_support/ios/task/text/nlclassifier:Sources/TFLBertNLClassifier.h",
        "//tensorflow_lite_support/ios/task/text/nlclassifier:Sources/TFLNLClassifier.h",
        "//tensorflow_lite_support/ios/task/text/qa:Sources/TFLBertQuestionAnswerer.h",
        "//tensorflow_lite_support/ios/task/vision:sources/TFLImageClassifier.h",
        "//tensorflow_lite_support/ios/task/vision:sources/TFLObjectDetector.h",
        "//tensorflow_lite_support/ios/task/vision:sources/TFLImageSegmenter.h",
        "//tensorflow_lite_support/ios/task/vision:sources/TFLImageSearcher.h",
        "//tensorflow_lite_support/ios:sources/TFLCommon.h",
        "//tensorflow_lite_support/ios/task/core:sources/TFLBaseOptions.h",
        "//tensorflow_lite_support/ios/task/core:sources/TFLExternalFile.h",
        "//tensorflow_lite_support/ios/task/processor:sources/TFLEmbeddingOptions.h",
        "//tensorflow_lite_support/ios/task/processor:sources/TFLSearchOptions.h",
        "//tensorflow_lite_support/ios/task/processor:sources/TFLClassificationOptions.h",
        "//tensorflow_lite_support/ios/task/processor:sources/TFLCategory.h",
        "//tensorflow_lite_support/ios/task/processor:sources/TFLClassificationResult.h",
        "//tensorflow_lite_support/ios/task/processor:sources/TFLDetectionResult.h",
        "//tensorflow_lite_support/ios/task/processor:sources/TFLSegmentationResult.h",
        "//tensorflow_lite_support/odml/ios/image:apis/GMLImage.h",
        "//tensorflow_lite_support/ios/task/processor:sources/TFLSearchResult.h",
        "//tensorflow_lite_support/ios/task/audio/core:sources/TFLAudioFormat.h",
        "//tensorflow_lite_support/ios/task/audio/core:sources/TFLFloatBuffer.h",
        "//tensorflow_lite_support/ios/task/audio/core/audio_record:sources/TFLAudioRecord.h",
        "//tensorflow_lite_support/ios/task/audio/core/audio_tensor:sources/TFLAudioTensor.h",
        "//tensorflow_lite_support/ios/task/audio:sources/TFLAudioClassifier.h",
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

objc_library(
    name = "TFLCommonCppUtils",
    srcs = [
        "sources/TFLCommonCppUtils.mm",
    ],
    hdrs = [
        "sources/TFLCommonCppUtils.h",
    ],
    copts = [
        "-ObjC++",
        "-std=c++17",
    ],
    module_name = "TFLCommonCppUtils",
    deps = [
        ":TFLCommon",
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/ios:TFLCommonUtils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

# bazel build -c opt --config=ios_arm64 //tensorflow_lite_support/ios:TensorFlowLiteTaskVision_framework
ios_static_framework(
    name = "TensorFlowLiteTaskVision_framework",
    hdrs = [
        ":GMLImage.h",
        ":TFLBaseOptions.h",
        ":TFLCategory.h",
        ":TFLClassificationOptions.h",
        ":TFLClassificationResult.h",
        ":TFLCommon.h",
        ":TFLDetectionResult.h",
        ":TFLEmbeddingOptions.h",
        ":TFLExternalFile.h",
        ":TFLImageClassifier.h",
        ":TFLImageSearcher.h",
        ":TFLImageSegmenter.h",
        ":TFLObjectDetector.h",
        ":TFLSearchOptions.h",
        ":TFLSearchResult.h",
        ":TFLSegmentationResult.h",
    ],
    bundle_name = "TensorFlowLiteTaskVision",
    minimum_os_version = TFL_TASK_MINIMUM_OS_VERSION,
    deps = [
        "//tensorflow_lite_support/ios/task/vision:TFLImageClassifier",
        "//tensorflow_lite_support/ios/task/vision:TFLImageSearcher",
        "//tensorflow_lite_support/ios/task/vision:TFLImageSegmenter",
        "//tensorflow_lite_support/ios/task/vision:TFLObjectDetector",
    ],
)

ios_static_framework(
    name = "TensorFlowLiteTaskText_framework",
    hdrs = [
        ":TFLBertNLClassifier.h",
        ":TFLBertQuestionAnswerer.h",
        ":TFLNLClassifier.h",
    ],
    bundle_name = "TensorFlowLiteTaskText",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    deps = [
        "//tensorflow_lite_support/ios/task/text/nlclassifier:TFLBertNLClassifier",
        "//tensorflow_lite_support/ios/task/text/nlclassifier:TFLNLClassifier",
        "//tensorflow_lite_support/ios/task/text/qa:TFLBertQuestionAnswerer",
    ],
)

ios_static_framework(
    name = "TensorFlowLiteTaskAudio_framework",
    hdrs = [
        ":TFLAudioClassifier.h",
        ":TFLAudioFormat.h",
        ":TFLAudioRecord.h",
        ":TFLAudioTensor.h",
        ":TFLBaseOptions.h",
        ":TFLCategory.h",
        ":TFLClassificationOptions.h",
        ":TFLClassificationResult.h",
        ":TFLCommon.h",
        ":TFLExternalFile.h",
        ":TFLFloatBuffer.h",
    ],
    bundle_name = "TensorFlowLiteTaskAudio",
    minimum_os_version = TFL_TASK_MINIMUM_OS_VERSION,
    deps = [
        "//tensorflow_lite_support/ios/task/audio:TFLAudioClassifier",
    ],
)
