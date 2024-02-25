load("//tensorflow_lite_support/cc/port:build_defs.bzl", "support_py_proto_library")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

proto_library(
    name = "nl_classifier_options_proto",
    srcs = ["nl_classifier_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
    ],
)

cc_proto_library(
    name = "nl_classifier_options_cc_proto",
    deps = [
        ":nl_classifier_options_proto",
    ],
)

cc_library(
    name = "nl_classifier_options_proto_inc",
    hdrs = ["nl_classifier_options_proto_inc.h"],
    deps = [
        ":nl_classifier_options_cc_proto",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
    ],
)

proto_library(
    name = "bert_nl_classifier_options_proto",
    srcs = ["bert_nl_classifier_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
    ],
)

cc_proto_library(
    name = "bert_nl_classifier_options_cc_proto",
    deps = [
        ":bert_nl_classifier_options_proto",
    ],
)

cc_library(
    name = "bert_nl_classifier_options_proto_inc",
    hdrs = ["bert_nl_classifier_options_proto_inc.h"],
    deps = [
        ":bert_nl_classifier_options_cc_proto",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
    ],
)

proto_library(
    name = "bert_question_answerer_options_proto",
    srcs = ["bert_question_answerer_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
    ],
)

cc_proto_library(
    name = "bert_question_answerer_options_cc_proto",
    deps = [
        ":bert_question_answerer_options_proto",
    ],
)

cc_library(
    name = "bert_question_answerer_options_proto_inc",
    hdrs = ["bert_question_answerer_options_proto_inc.h"],
    deps = [
        ":bert_question_answerer_options_cc_proto",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
    ],
)

proto_library(
    name = "retrieval_proto",
    srcs = ["retrieval.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_proto",
    ],
)

cc_proto_library(
    name = "retrieval_cc_proto",
    deps = [
        ":retrieval_proto",
    ],
)

cc_library(
    name = "retrieval_proto_inc",
    hdrs = ["retrieval_proto_inc.h"],
    deps = [
        ":retrieval_cc_proto",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:embeddings_proto_inc",
    ],
)

proto_library(
    name = "text_embedder_options_proto",
    srcs = ["text_embedder_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_proto",
    ],
)

cc_proto_library(
    name = "text_embedder_options_cc_proto",
    deps = [":text_embedder_options_proto"],
)

support_py_proto_library(
    name = "text_embedder_options_py_pb2",
    srcs = ["text_embedder_options.proto"],
    api_version = 2,
    proto_deps = [":text_embedder_options_proto"],
    py_proto_deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_py_pb2",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_py_pb2",
    ],
)

proto_library(
    name = "clu_proto",
    srcs = ["clu.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:class_proto",
    ],
)

cc_proto_library(
    name = "clu_cc_proto",
    deps = [":clu_proto"],
)

cc_library(
    name = "clu_proto_inc",
    hdrs = ["clu_proto_inc.h"],
    deps = [":clu_cc_proto"],
)

proto_library(
    name = "bert_clu_annotator_options_proto",
    srcs = ["bert_clu_annotator_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
    ],
)

cc_proto_library(
    name = "bert_clu_annotator_options_cc_proto",
    deps = [":bert_clu_annotator_options_proto"],
)

cc_library(
    name = "bert_clu_annotator_options_proto_inc",
    hdrs = ["bert_clu_annotator_options_proto_inc.h"],
    deps = [
        ":bert_clu_annotator_options_cc_proto",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
    ],
)

proto_library(
    name = "text_searcher_options_proto",
    srcs = ["text_searcher_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_options_proto",
    ],
)

cc_proto_library(
    name = "text_searcher_options_cc_proto",
    deps = [
        ":text_searcher_options_proto",
    ],
)
