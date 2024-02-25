load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")
load("@org_tensorflow//tensorflow/lite/ios:ios.bzl", "TFL_DEFAULT_TAGS", "TFL_DISABLED_SANITIZER_TAGS", "TFL_MINIMUM_OS_VERSION")
load("@build_bazel_rules_apple//apple:ios.bzl", "ios_unit_test")
load("@org_tensorflow//tensorflow/lite:special_rules.bzl", "tflite_ios_lab_runner")

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLObjectDetectorObjcTestLibrary",
    testonly = 1,
    srcs = ["TFLObjectDetectorTests.m"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        "//tensorflow_lite_support/ios/task/vision:TFLObjectDetector",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageUtils",
    ],
)

ios_unit_test(
    name = "TFLObjectDetectorObjcTest",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLObjectDetectorObjcTestLibrary",
    ],
)

swift_library(
    name = "TFLObjectDetectorSwiftTestLibrary",
    testonly = 1,
    srcs = ["TFLObjectDetectorTests.swift"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        "//tensorflow_lite_support/ios/task/vision:TFLObjectDetector",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageUtils",
    ],
)

ios_unit_test(
    name = "TFLObjectDetectorSwiftTest",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLObjectDetectorSwiftTestLibrary",
    ],
)
