load(
    "@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl",
    "cc_library_with_tflite",
)

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library_with_tflite(
    name = "bert_nl_classifier",
    srcs = [
        "bert_nl_classifier.cc",
    ],
    hdrs = [
        "bert_nl_classifier.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/processor:bert_preprocessor",
        "//tensorflow_lite_support/cc/task/text/utils:bert_utils",
    ],
    visibility = ["//tensorflow_lite_support:users"],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/task/core:category",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/text/proto:bert_nl_classifier_options_proto_inc",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@org_tensorflow//tensorflow/lite:string",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api",
    ],
)

cc_library_with_tflite(
    name = "question_answerer",
    hdrs = [
        "question_answerer.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/core:base_task_api",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
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
        ":question_answerer",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "//tensorflow_lite_support/cc/task/core:base_task_api",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/text/proto:bert_question_answerer_options_proto_inc",
        "//tensorflow_lite_support/cc/text/tokenizers:bert_tokenizer",
        "//tensorflow_lite_support/cc/text/tokenizers:sentencepiece_tokenizer",
        "//tensorflow_lite_support/cc/text/tokenizers:tokenizer",
        "//tensorflow_lite_support/cc/text/tokenizers:tokenizer_utils",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library_with_tflite(
    name = "universal_sentence_encoder_qa",
    srcs = [
        "universal_sentence_encoder_qa.cc",
    ],
    hdrs = [
        "universal_sentence_encoder_qa.h",
    ],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "//tensorflow_lite_support/cc/task/core:base_task_api",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
        "//tensorflow_lite_support/cc/task/text/utils:universal_sentence_encoder_utils",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_cc_proto",
        "//tensorflow_lite_support/cc/task/text/proto:retrieval_cc_proto",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library_with_tflite(
    name = "text_embedder",
    srcs = [
        "text_embedder.cc",
    ],
    hdrs = [
        "text_embedder.h",
    ],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "//tensorflow_lite_support/cc/task/core:base_task_api",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/processor:embedding_postprocessor",
        "//tensorflow_lite_support/cc/task/processor:text_preprocessor",
        "//tensorflow_lite_support/cc/task/processor:bert_preprocessor",
        "//tensorflow_lite_support/cc/task/processor:regex_preprocessor",
        "//tensorflow_lite_support/cc/task/processor:universal_sentence_encoder_preprocessor",
        "//tensorflow_lite_support/cc/task/text/utils:bert_utils",
        "//tensorflow_lite_support/cc/task/text/utils:universal_sentence_encoder_utils",
    ],
    visibility = ["//tensorflow_lite_support:users"],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_cc_proto",
        "//tensorflow_lite_support/cc/task/text/proto:text_embedder_options_cc_proto",
        "@com_google_absl//absl/status",
        "@org_tensorflow//tensorflow/lite/core/api:op_resolver",
    ],
)

cc_library_with_tflite(
    name = "clu_annotator",
    hdrs = [
        "clu_annotator.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/core:base_task_api",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
    ],
    deps = [
        "//tensorflow_lite_support/cc/task/text/proto:clu_proto_inc",
    ],
)

cc_library_with_tflite(
    name = "bert_clu_annotator",
    srcs = [
        "bert_clu_annotator.cc",
    ],
    hdrs = [
        "bert_clu_annotator.h",
    ],
    tflite_deps = [
        ":clu_annotator",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/text/clu_lib:tflite_modules",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/text/proto:bert_clu_annotator_options_proto_inc",
        "//tensorflow_lite_support/cc/text/tokenizers:bert_tokenizer",
        "//tensorflow_lite_support/cc/text/tokenizers:tokenizer_utils",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library_with_tflite(
    name = "text_searcher",
    srcs = ["text_searcher.cc"],
    hdrs = ["text_searcher.h"],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "//tensorflow_lite_support/cc/task/core:base_task_api",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
        "//tensorflow_lite_support/cc/task/processor:bert_preprocessor",
        "//tensorflow_lite_support/cc/task/processor:regex_preprocessor",
        "//tensorflow_lite_support/cc/task/processor:search_postprocessor",
        "//tensorflow_lite_support/cc/task/processor:text_preprocessor",
        "//tensorflow_lite_support/cc/task/processor:universal_sentence_encoder_preprocessor",
        "//tensorflow_lite_support/cc/task/text/utils:bert_utils",
        "//tensorflow_lite_support/cc/task/text/utils:universal_sentence_encoder_utils",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_result_cc_proto",
        "//tensorflow_lite_support/cc/task/text/proto:text_searcher_options_cc_proto",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api:op_resolver",
    ],
)
