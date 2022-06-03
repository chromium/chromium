package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "bert_qa_c_api.h",
])

cc_library(
    name = "question_answerer",
    hdrs = [
        "question_answerer.h",
    ],
    deps = [
        "//tensorflow_lite_support/cc/task/core:base_task_api",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
    ],
)

cc_library(
    name = "bert_question_answerer",
    srcs = [
        "bert_question_answerer.cc",
    ],
    hdrs = [
        "bert_question_answerer.h",
    ],
    deps = [
        ":question_answerer",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:base_task_api",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
        "//tensorflow_lite_support/cc/text/tokenizers:bert_tokenizer",
        "//tensorflow_lite_support/cc/text/tokenizers:sentencepiece_tokenizer",
        "//tensorflow_lite_support/cc/text/tokenizers:tokenizer",
        "//tensorflow_lite_support/cc/text/tokenizers:tokenizer_utils",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "bert_qa_c_api",
    srcs = [
        "bert_qa_c_api.cc",
    ],
    hdrs = [
        "bert_qa_c_api.h",
    ],
    visibility = ["//tensorflow_lite_support:__subpackages__"],
    deps = [
        ":bert_question_answerer",
        ":question_answerer",
    ],
)
