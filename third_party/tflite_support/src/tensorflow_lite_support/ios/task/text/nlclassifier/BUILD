load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")
load("@org_tensorflow//tensorflow/lite/experimental/ios:ios.bzl", "TFL_DEFAULT_TAGS", "TFL_DISABLED_SANITIZER_TAGS", "TFL_MINIMUM_OS_VERSION")
load("@build_bazel_rules_apple//apple:ios.bzl", "ios_unit_test")
load("@org_tensorflow//tensorflow/lite:special_rules.bzl", "tflite_ios_lab_runner")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLBertNLClassifier",
    srcs = ["Sources/TFLBertNLClassifier.m"],
    hdrs = ["Sources/TFLBertNLClassifier.h"],
    module_name = "TFLBertNLClassifier",
    deps = [
        "//tensorflow_lite_support/cc/task/text/nlclassifier:bert_nl_classifier_c_api",
        "@google_toolbox_for_mac//:GTM_Defines",
    ],
)

swift_library(
    name = "TFLBertNLClassifierSwiftTestLibrary",
    testonly = 1,
    srcs = ["Tests/TFLBertNLClassifierTest.swift"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:nl_classifier_models",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        ":TFLBertNLClassifier",
        "//third_party/swift/xctest",
    ],
)

ios_unit_test(
    name = "TFLBertNLClassifierSwiftTest",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLBertNLClassifierSwiftTestLibrary",
    ],
)

objc_library(
    name = "TFLBertNLClassifierObjcTestLibrary",
    testonly = 1,
    srcs = ["Tests/TFLBertNLClassifierTest.m"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:nl_classifier_models",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        ":TFLBertNLClassifier",
    ],
)

ios_unit_test(
    name = "TFLBertNLClassifierObjcTest",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLBertNLClassifierObjcTestLibrary",
    ],
)

objc_library(
    name = "TFLNLClassifier",
    srcs = ["Sources/TFLNLClassifier.m"],
    hdrs = ["Sources/TFLNLClassifier.h"],
    module_name = "TFLNLClassifier",
    deps = [
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier_c_api",
        "@google_toolbox_for_mac//:GTM_Defines",
    ],
)

swift_library(
    name = "TFLNLClassifierSwiftTestLibrary",
    testonly = 1,
    srcs = ["Tests/TFLNLClassifierTest.swift"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:nl_classifier_models",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        ":TFLNLClassifier",
        "//third_party/swift/xctest",
    ],
)

ios_unit_test(
    name = "TFLNLClassifierSwiftTest",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLNLClassifierSwiftTestLibrary",
    ],
)

objc_library(
    name = "TFLNLClassifierObjcTestLibrary",
    testonly = 1,
    srcs = ["Tests/TFLNLClassifierTest.m"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:nl_classifier_models",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        ":TFLNLClassifier",
    ],
)

ios_unit_test(
    name = "TFLNLClassifierObjcTest",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLNLClassifierObjcTestLibrary",
    ],
)
