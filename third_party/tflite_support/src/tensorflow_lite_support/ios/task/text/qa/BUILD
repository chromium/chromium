load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")
load("@org_tensorflow//tensorflow/lite/ios:ios.bzl", "TFL_DEFAULT_TAGS", "TFL_DISABLED_SANITIZER_TAGS", "TFL_MINIMUM_OS_VERSION")
load("@build_bazel_rules_apple//apple:ios.bzl", "ios_unit_test")
load("@org_tensorflow//tensorflow/lite:special_rules.bzl", "tflite_ios_lab_runner")

package(
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLBertQuestionAnswerer",
    srcs = ["Sources/TFLBertQuestionAnswerer.m"],
    hdrs = ["Sources/TFLBertQuestionAnswerer.h"],
    module_name = "TFLBertQuestionAnswerer",
    visibility = ["//tensorflow_lite_support:internal"],
    deps = [
        "//tensorflow_lite_support/c/task/text:bert_question_answerer",
        "@google_toolbox_for_mac//:GTM_Defines",
    ],
)

swift_library(
    name = "TFLBertQuestionAnswererSwiftTestLibrary",
    testonly = 1,
    srcs = glob(["Tests/*.swift"]),
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:albert_model",
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobile_bert_model",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [":TFLBertQuestionAnswerer"],
)

ios_unit_test(
    name = "TFLBertQuestionAnswererSwiftTest",
    size = "large",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLBertQuestionAnswererSwiftTestLibrary",
    ],
)

objc_library(
    name = "TFLBertQuestionAnswererObjcTestLibrary",
    testonly = 1,
    srcs = glob(["Tests/*.m"]),
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:albert_model",
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobile_bert_model",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [
        ":TFLBertQuestionAnswerer",
    ],
)

ios_unit_test(
    name = "TFLBertQuestionAnswererObjcTest",
    size = "large",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLBertQuestionAnswererObjcTestLibrary",
    ],
)
