package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

# Example usage:
# bazel run -c opt \
#  tensorflow_lite_support/examples/task/text/desktop:bert_question_answerer_demo \
#  -- \
#  --model_path=/path/to/model.tflite \
#  --question="question to ask" \
#  --context="context for the question to ask"
cc_binary(
    name = "bert_question_answerer_demo",
    srcs = ["bert_question_answerer_demo.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/text:bert_question_answerer",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ] + select({
        "//tensorflow_lite_support/examples/task:darwinn_portable": [
            "//tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin",
        ],
        "//conditions:default": [
        ],
    }),
)

# Example usage:
# bazel run -c opt \
#  tensorflow_lite_support/examples/task/text/desktop:bert_nl_classifier_demo \
#  -- \
#  --model_path=/path/to/model.tflite \
#  --text="text to classify"
cc_binary(
    name = "bert_nl_classifier_demo",
    srcs = ["bert_nl_classifier_demo.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:category",
        "//tensorflow_lite_support/cc/task/text:bert_nl_classifier",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ] + select({
        "//tensorflow_lite_support/examples/task:darwinn_portable": [
            "//tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin",
        ],
        "//conditions:default": [
        ],
    }),
)

# Example usage:
# bazel run -c opt \
#  tensorflow_lite_support/examples/task/text/desktop:nl_classifier_demo \
#  -- \
#  --model_path=/path/to/model.tflite \
#  --text="text to classify" \
#  --input_tensor_name="input_text" \
#  --output_score_tensor_name="probability"
cc_binary(
    name = "nl_classifier_demo",
    srcs = ["nl_classifier_demo.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:category",
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ] + select({
        "//tensorflow_lite_support/examples/task:darwinn_portable": [
            "//tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin",
        ],
        "//conditions:default": [
        ],
    }),
)

# Example usage:
# bazel run -c opt \
#  tensorflow_lite_support/examples/task/text/desktop:universal_sentence_encoder_qa_main \
#  -- \
#  --model_path=/path/to/model_with_metadata.tflite
cc_binary(
    name = "universal_sentence_encoder_qa_demo",
    srcs = [
        "universal_sentence_encoder_qa_demo.cc",
    ],
    deps = [
        "//tensorflow_lite_support/cc/task/text:universal_sentence_encoder_qa",
        "//tensorflow_lite_support/cc/task/text/utils:text_op_resolver",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

# Example usage:
# bazel run -c opt \
#  tensorflow_lite_support/examples/task/text/desktop:text_searcher_demo \
#  -- \
#  --model_path=/path/to/model.tflite \
#  --index_path=/path/to/index.ldb \
#  --input_sentence="your_input"
cc_binary(
    name = "text_searcher_demo",
    srcs = ["text_searcher_demo.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:configuration_proto_inc",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_cc_proto",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_result_cc_proto",
        "//tensorflow_lite_support/cc/task/text:text_searcher",
        "//tensorflow_lite_support/cc/task/text/proto:text_searcher_options_cc_proto",
        "//tensorflow_lite_support/cc/task/text/utils:text_op_resolver",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ],
)

# Example usage:
# bazel run -c opt \
#  tensorflow_lite_support/examples/task/text/desktop:text_embedder_demo \
#  -- \
#  --model_path=/path/to/model.tflite \
#  --first_sentence="first sentence" \
#  --second_sentence="second sentence"
cc_binary(
    name = "text_embedder_demo",
    srcs = ["text_embedder_demo.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:configuration_proto_inc",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_cc_proto",
        "//tensorflow_lite_support/cc/task/text:text_embedder",
        "//tensorflow_lite_support/cc/task/text/proto:text_embedder_options_cc_proto",
        "//tensorflow_lite_support/cc/task/text/utils:text_op_resolver",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ],
)
