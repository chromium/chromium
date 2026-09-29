load(
    "@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl",
    "cc_test_with_tflite",
)
load("//third_party/bazel_rules/rules_cc/cc:cc_test.bzl", "cc_test")

package(
    default_visibility = [
        "//visibility:private",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_test_with_tflite(
    name = "bert_nl_classifier_test",
    srcs = ["bert_nl_classifier_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:bert_nl_classifier_models",
    ],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite:test_util",
        "//tensorflow_lite_support/cc/task/text:bert_nl_classifier",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/test:test_utils",
    ],
)

cc_test(
    name = "bert_question_answerer_test",
    timeout = "long",
    srcs = ["bert_question_answerer_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:albert_model",
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobile_bert_model",
    ],
    tags = [
        "optonly",  # The test takes long, and only run with -c opt.
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/text:bert_question_answerer",
        "//tensorflow_lite_support/cc/test:test_utils",
        "@org_tensorflow//tensorflow/lite:test_util",
    ],
)

cc_test(
    name = "universal_sentence_encoder_qa_test",
    timeout = "long",
    srcs = ["universal_sentence_encoder_qa_test.cc"],
    data = ["//tensorflow_lite_support/cc/test/testdata/task/text:universal_sentence_encoder_qa"],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/text:universal_sentence_encoder_qa",
        "//tensorflow_lite_support/cc/task/text/proto:retrieval_cc_proto",
        "//tensorflow_lite_support/cc/task/text/utils:text_op_resolver",
        "//tensorflow_lite_support/cc/test:test_utils",
        "@org_tensorflow//tensorflow/lite:test_util",
    ],
)

cc_test(
    name = "bert_clu_annotator_test",
    timeout = "long",
    srcs = ["bert_clu_annotator_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:bert_clu_annotator_with_metadata",
    ],
    tags = [
        "optonly",  # The test takes long, and only run with -c opt.
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port/default:status_matchers",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/text:bert_clu_annotator",
        "//tensorflow_lite_support/cc/task/text/proto:clu_proto_inc",
        "//tensorflow_lite_support/cc/test:test_utils",
    ],
)
