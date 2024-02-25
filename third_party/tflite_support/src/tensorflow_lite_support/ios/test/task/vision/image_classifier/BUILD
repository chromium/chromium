load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")
load("@org_tensorflow//tensorflow/lite/ios:ios.bzl", "TFL_DEFAULT_TAGS", "TFL_DISABLED_SANITIZER_TAGS")
load("@build_bazel_rules_apple//apple:ios.bzl", "ios_unit_test")
load("@org_tensorflow//tensorflow/lite:special_rules.bzl", "tflite_ios_lab_runner")
load(
    "//tensorflow_lite_support/ios:ios.bzl",
    "TFL_TASK_MINIMUM_OS_VERSION",
)

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

swift_library(
    name = "TFLImageClassifierSwiftTestLibrary",
    testonly = 1,
    srcs = ["TFLImageClassifierTests.swift"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        "//tensorflow_lite_support/ios/task/vision:TFLImageClassifier",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageUtils",
    ],
)

ios_unit_test(
    name = "TFLImageClassifierSwiftTest",
    minimum_os_version = TFL_TASK_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLImageClassifierSwiftTestLibrary",
    ],
)

objc_library(
    name = "TFLImageClassifierObjcTestLibrary",
    testonly = 1,
    srcs = ["TFLImageClassifierTests.m"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        "//tensorflow_lite_support/ios/task/vision:TFLImageClassifier",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageUtils",
    ],
)

ios_unit_test(
    name = "TFLImageClassifierObjcTest",
    minimum_os_version = TFL_TASK_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLImageClassifierObjcTestLibrary",
    ],
)

objc_library(
    name = "TFLImageClassifierCoreMLDelegateTestLibrary",
    testonly = 1,
    srcs = ["TFLImageClassifierCoreMLDelegateTest.mm"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        "//tensorflow_lite_support/c/task/vision/utils:frame_buffer_cpp_c_utils",
        "//tensorflow_lite_support/cc/task/vision:image_classifier",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptions",
        "//tensorflow_lite_support/ios/task/vision:TFLImageClassifier",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageUtils",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:coreml_plugin",
    ],
)

ios_unit_test(
    name = "TFLImageClassifierCoreMLDelegateTest",
    minimum_os_version = TFL_TASK_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLImageClassifierCoreMLDelegateTestLibrary",
    ],
)
