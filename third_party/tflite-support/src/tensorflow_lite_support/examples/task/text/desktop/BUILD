package(
    default_visibility = [
        "//tensorflow_lite_support:users",
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
        "//tensorflow_lite_support/cc/task/text/qa:bert_question_answerer",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ],
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
        "//tensorflow_lite_support/cc/task/text/nlclassifier:bert_nl_classifier",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ],
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
    ],
)
