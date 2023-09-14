# Placeholder for internal Python strict test compatibility macro.

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

py_test(
    name = "text_embedder_test",
    srcs = ["text_embedder_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobilebert_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:regex_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:universal_sentence_encoder_qa",
    ],
    tags = ["not_run:arm"],
    deps = [
        # build rule placeholder: numpy dep,
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/text:text_embedder",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "text_searcher_test",
    srcs = ["text_searcher_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobilebert_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:regex_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:test_indices",
        "//tensorflow_lite_support/cc/test/testdata/task/text:test_searchers",
        "//tensorflow_lite_support/cc/test/testdata/task/text:universal_sentence_encoder_qa",
    ],
    tags = ["not_run:arm"],
    deps = [
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:search_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:search_result_pb2",
        "//tensorflow_lite_support/python/task/text:text_searcher",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "nl_classifier_test",
    srcs = ["nl_classifier_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:nl_classifier_models",
    ],
    deps = [
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:class_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classification_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classifications_pb2",
        "//tensorflow_lite_support/python/task/text:nl_classifier",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "bert_nl_classifier_test",
    srcs = ["bert_nl_classifier_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:bert_nl_classifier_models",
    ],
    tags = ["not_run:arm"],
    deps = [
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:class_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classifications_pb2",
        "//tensorflow_lite_support/python/task/text:bert_nl_classifier",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "bert_question_answerer_test",
    srcs = ["bert_question_answerer_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:albert_model",
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobile_bert_model",
    ],
    deps = [
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:qa_answers_pb2",
        "//tensorflow_lite_support/python/task/text:bert_question_answerer",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "bert_clu_annotator_test",
    srcs = ["bert_clu_annotator_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:bert_clu_annotator_with_metadata",
    ],
    tags = ["not_run:arm"],
    deps = [
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:class_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:clu_annotation_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:clu_pb2",
        "//tensorflow_lite_support/python/task/text:bert_clu_annotator",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)
