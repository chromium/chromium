load("@build_bazel_rules_swift//swift:swift.bzl", "swift_library")
load("@org_tensorflow//tensorflow/lite/ios:ios.bzl", "TFL_DEFAULT_TAGS", "TFL_DISABLED_SANITIZER_TAGS", "TFL_MINIMUM_OS_VERSION")
load("@build_bazel_rules_apple//apple:ios.bzl", "ios_unit_test")
load("@org_tensorflow//tensorflow/lite:special_rules.bzl", "tflite_ios_lab_runner")

package(
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLTokenizerUtil",
    srcs = [
        "Sources/TFLTokenizerUtil.mm",
    ],
    hdrs = [
        "Sources/TFLTokenizerUtil.h",
    ],
    module_name = "TFLTokenizerUtil",
    deps = [
        "//tensorflow_lite_support/cc/text/tokenizers:tokenizer",
        "//tensorflow_lite_support/ios/utils:TFLStringUtil",
    ],
)

objc_library(
    name = "TFLBertTokenizer",
    srcs = [
        "Sources/TFLBertTokenizer.mm",
    ],
    hdrs = [
        "Sources/TFLBertTokenizer.h",
        "Sources/TFLTokenizer.h",
    ],
    module_name = "TFLBertTokenizer",
    deps = [
        ":TFLTokenizerUtil",
        "//tensorflow_lite_support/cc/text/tokenizers:bert_tokenizer",
        "//tensorflow_lite_support/ios/utils:TFLStringUtil",
    ],
)

swift_library(
    name = "TFLBertTokenizerTestLibrary",
    testonly = 1,
    srcs = ["Tests/TFLBertTokenizerTest.swift"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobile_bert_model",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [":TFLBertTokenizer"],
)

ios_unit_test(
    name = "TFLBertTokenizerTest",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLBertTokenizerTestLibrary",
    ],
)

objc_library(
    name = "TFLSentencepieceTokenizer",
    srcs = [
        "Sources/TFLSentencepieceTokenizer.mm",
    ],
    hdrs = [
        "Sources/TFLSentencepieceTokenizer.h",
        "Sources/TFLTokenizer.h",
    ],
    features = ["-layering_check"],
    module_name = "TFLSentencepieceTokenizer",
    deps = [
        ":TFLTokenizerUtil",
        "//tensorflow_lite_support/cc/text/tokenizers:sentencepiece_tokenizer",
        "//tensorflow_lite_support/ios/utils:TFLStringUtil",
    ],
)

swift_library(
    name = "TFLSentencepieceTokenizerTestLibrary",
    testonly = 1,
    srcs = ["Tests/TFLSentencepieceTokenizerTest.swift"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:albert_model",
    ],
    tags = TFL_DEFAULT_TAGS,
    deps = [":TFLSentencepieceTokenizer"],
)

ios_unit_test(
    name = "TFLSentencepieceTokenizerTest",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    tags = TFL_DEFAULT_TAGS + TFL_DISABLED_SANITIZER_TAGS,
    deps = [
        ":TFLSentencepieceTokenizerTestLibrary",
    ],
)
