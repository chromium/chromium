load("@build_bazel_rules_apple//apple:ios.bzl", "ios_unit_test")
load("@org_tensorflow//tensorflow/lite:special_rules.bzl", "tflite_ios_lab_runner")
load("@org_tensorflow//tensorflow/lite/ios:ios.bzl", "TFL_DEFAULT_TAGS", "TFL_DISABLED_SANITIZER_TAGS")
load("//third_party/bazel_rules/rules_cc/cc:objc_library.bzl", "objc_library")

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLRingBufferObjcTestLibrary",
    testonly = 1,
    srcs = ["TFLRingBufferTests.m"],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        "//tensorflow_lite_support/ios:TFLCommon",
        "//tensorflow_lite_support/ios/task/audio/core:TFLRingBuffer",
    ],
)

ios_unit_test(
    name = "TFLRingBufferObjcTest",
    minimum_os_version = "15.0",
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLRingBufferObjcTestLibrary",
    ],
)
