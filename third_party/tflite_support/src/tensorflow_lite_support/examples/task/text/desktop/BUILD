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
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/text:bert_question_answerer",
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
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:category",
        "//tensorflow_lite_support/cc/task/text:bert_nl_classifier",
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
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:category",
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier",
    ] + select({
        "//tensorflow_lite_support/examples/task:darwinn_portable": [
            "//tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin",
        ],
        "//conditions:default": [
        ],
    }),
)

cc_library(
    name = "universal_sentence_encoder_qa_op_resolver",
    srcs = ["universal_sentence_encoder_qa_op_resolver.cc"],
    hdrs = ["universal_sentence_encoder_qa_op_resolver.h"],
    deps = [
        "//tensorflow_lite_support/custom_ops/kernel/ragged:ragged_tensor_to_tensor_tflite",  # fixdeps: keep
        "//tensorflow_lite_support/custom_ops/kernel/sentencepiece:sentencepiece_tokenizer_tflite",  # fixdeps: keep
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite:op_resolver",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
    ],
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
        ":universal_sentence_encoder_qa_op_resolver",
        "//tensorflow_lite_support/cc/task/text:universal_sentence_encoder_qa",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)
