load(
    "@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl",
    "cc_library_with_tflite",
)

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "bert_nl_classifier.h",
    "nl_classifier.h",
    "nl_classifier_common.h",
    "bert_question_answerer.h",
])

cc_library_with_tflite(
    name = "nl_classifier",
    srcs = [
        "nl_classifier.cc",
    ],
    hdrs = [
        "nl_classifier.h",
        "nl_classifier_common.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier",
    ],
    deps = [
        ":nl_classifier_common",
        "//tensorflow_lite_support/cc/task/core:category",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_with_tflite(
    name = "bert_nl_classifier",
    srcs = [
        "bert_nl_classifier.cc",
    ],
    hdrs = [
        "bert_nl_classifier.h",
        "nl_classifier_common.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/text:bert_nl_classifier",
    ],
    deps = [
        ":nl_classifier_common",
        "//tensorflow_lite_support/cc/task/core:category",
        "//tensorflow_lite_support/cc/task/text/proto:bert_nl_classifier_options_proto_inc",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "nl_classifier_common",
    srcs = [
        "nl_classifier_common.cc",
    ],
    hdrs = [
        "nl_classifier_common.h",
    ],
)

cc_library_with_tflite(
    name = "bert_question_answerer",
    srcs = [
        "bert_question_answerer.cc",
    ],
    hdrs = [
        "bert_question_answerer.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/text:bert_question_answerer",
        "//tensorflow_lite_support/cc/task/text:question_answerer",
    ],
)
