# Placeholder: load py_test

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

py_test(
    name = "nl_classifier_test",
    srcs = ["nl_classifier_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:nl_classifier_models",
    ],
    deps = [
        # build rule placeholder: google/protobuf:use_fast_cpp_protos dep,  # Automatically added go/proto_python_upb_flip
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
    name = "bert_question_answerer_test",
    srcs = ["bert_question_answerer_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:albert_model",
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobile_bert_model",
    ],
    deps = [
        # build rule placeholder: google/protobuf:use_fast_cpp_protos dep,  # Automatically added go/proto_python_upb_flip
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
        # build rule placeholder: google/protobuf:use_fast_cpp_protos dep,  # Automatically added go/proto_python_upb_flip
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
