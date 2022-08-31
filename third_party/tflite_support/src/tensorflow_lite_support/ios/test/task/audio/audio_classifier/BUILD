load("@org_tensorflow//tensorflow/lite/ios:ios.bzl", "TFL_DEFAULT_TAGS", "TFL_DISABLED_SANITIZER_TAGS", "TFL_MINIMUM_OS_VERSION")
load("@build_bazel_rules_apple//apple:ios.bzl", "ios_unit_test")
load("@org_tensorflow//tensorflow/lite:special_rules.bzl", "tflite_ios_lab_runner")
load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLAudioClassifierObjcTestLibrary",
    testonly = 1,
    srcs = ["TFLAudioClassifierTests.m"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/audio:test_audio_clips",
        "//tensorflow_lite_support/cc/test/testdata/task/audio:test_models",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        "//tensorflow_lite_support/ios:TFLCommon",
        "//tensorflow_lite_support/ios/task/audio:TFLAudioClassifier",
        "//tensorflow_lite_support/ios/test/task/audio/core/audio_record/utils:AVAudioPCMBufferUtils",
    ],
)

ios_unit_test(
    name = "TFLAudioClassifierObjcTest",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLAudioClassifierObjcTestLibrary",
    ],
)

swift_library(
    name = "TFLAudioClassifierSwiftTestLibrary",
    testonly = 1,
    srcs = ["TFLAudioClassifierTests.swift"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/audio:test_audio_clips",
        "//tensorflow_lite_support/cc/test/testdata/task/audio:test_models",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        "//tensorflow_lite_support/ios:TFLCommon",
        "//tensorflow_lite_support/ios/task/audio:TFLAudioClassifier",
        "//tensorflow_lite_support/ios/test/task/audio/core/audio_record/utils:AVAudioPCMBufferUtils",
    ],
)

ios_unit_test(
    name = "TFLAudioClassifierSwiftTest",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLAudioClassifierSwiftTestLibrary",
    ],
)
