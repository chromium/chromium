load(
    "@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl",
    "cc_test_with_tflite",
)

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

cc_test(
    name = "text_embedder_test",
    srcs = ["text_embedder_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobilebert_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:regex_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:universal_sentence_encoder_qa",
    ],
    tags = ["not_run:arm"],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_cc_proto",
        "//tensorflow_lite_support/cc/task/text:text_embedder",
        "//tensorflow_lite_support/cc/task/text/utils:text_op_resolver",
        "//tensorflow_lite_support/cc/test:test_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite:test_util",
    ],
)

cc_test(
    name = "text_searcher_test",
    srcs = ["text_searcher_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobilebert_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:regex_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:test_indices",
        "//tensorflow_lite_support/cc/test/testdata/task/text:test_searchers",
        "//tensorflow_lite_support/cc/test/testdata/task/text:universal_sentence_encoder_qa",
    ],
    tags = ["not_run:arm"],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_result_cc_proto",
        "//tensorflow_lite_support/cc/task/text:text_searcher",
        "//tensorflow_lite_support/cc/task/text/proto:text_searcher_options_cc_proto",
        "//tensorflow_lite_support/cc/task/text/utils:text_op_resolver",
        "//tensorflow_lite_support/cc/test:test_utils",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@org_tensorflow//tensorflow/lite:test_util",
        "@org_tensorflow//tensorflow/lite/core/api:op_resolver",
    ],
)
