# Placeholder for internal Python strict library compatibility macro.

package(
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "text_embedder",
    srcs = [
        "text_embedder.py",
    ],
    visibility = ["//tensorflow_lite_support:users"],
    deps = [
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_pb2",
        "//tensorflow_lite_support/python/task/text/pybinds:_pywrap_text_embedder",
    ],
)

py_library(
    name = "text_searcher",
    srcs = [
        "text_searcher.py",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:search_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:search_result_pb2",
        "//tensorflow_lite_support/python/task/text/pybinds:_pywrap_text_searcher",
    ],
)

py_library(
    name = "nl_classifier",
    srcs = [
        "nl_classifier.py",
    ],
    visibility = ["//tensorflow_lite_support:users"],
    deps = [
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:classification_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classifications_pb2",
        "//tensorflow_lite_support/python/task/text/pybinds:_pywrap_nl_classifier",
    ],
)

py_library(
    name = "bert_nl_classifier",
    srcs = [
        "bert_nl_classifier.py",
    ],
    visibility = ["//tensorflow_lite_support:users"],
    deps = [
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:classifications_pb2",
        "//tensorflow_lite_support/python/task/text/pybinds:_pywrap_bert_nl_classifier",
    ],
)

py_library(
    name = "bert_question_answerer",
    srcs = [
        "bert_question_answerer.py",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:qa_answers_pb2",
        "//tensorflow_lite_support/python/task/text/pybinds:_pywrap_bert_question_answerer",
    ],
)

py_library(
    name = "bert_clu_annotator",
    srcs = [
        "bert_clu_annotator.py",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:clu_annotation_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:clu_pb2",
        "//tensorflow_lite_support/python/task/text/pybinds:_pywrap_bert_clu_annotator",
    ],
)
