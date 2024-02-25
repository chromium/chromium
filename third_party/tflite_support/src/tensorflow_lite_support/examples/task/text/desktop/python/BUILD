# Placeholder: load py_binary

package(
    licenses = ["notice"],  # Apache 2.0
)

# bazel run \
# tensorflow_lite_support/examples/task/text/desktop/python:nl_classifier_demo \
# -- \
# --model_path=/path/to/model.tflite \
# --text="What a waste of my time."
py_binary(
    name = "nl_classifier_demo",
    srcs = ["nl_classifier_demo.py"],
    data = ["//tensorflow_lite_support/examples/task/text/desktop:nl_classifier_demo"],
    deps = [
        # build rule placeholder: tensorflow dep,
        "@absl_py//absl:app",
        "@absl_py//absl/flags",
    ],
)

# bazel run \
# tensorflow_lite_support/examples/task/text/desktop/python:bert_nl_classifier_demo \
# -- \
# --model_path=/path/to/model.tflite \
# --text="it's a charming and often affecting journey"
py_binary(
    name = "bert_nl_classifier_demo",
    srcs = ["bert_nl_classifier_demo.py"],
    data = ["//tensorflow_lite_support/examples/task/text/desktop:bert_nl_classifier_demo"],
    deps = [
        # build rule placeholder: tensorflow dep,
        "@absl_py//absl:app",
        "@absl_py//absl/flags",
    ],
)

# bazel run \
#  tensorflow_lite_support/examples/task/text/desktop/python:bert_question_answerer_demo -- \
#  --model_path=/tmp/mobilebert.tflite \
#  --question="Where is Amazon rainforest?" \
#  --context="The Amazon rainforest, alternatively, the Amazon Jungle, also known in \
# English as Amazonia, is a moist broadleaf tropical rainforest in the Amazon \
# biome that covers most of the Amazon basin of South America. This basin \
# encompasses 7,000,000 km2 (2,700,000 sq mi), of which \
# 5,500,000 km2 (2,100,000 sq mi) are covered by the rainforest. This region \
# includes territory belonging to nine nations."
py_binary(
    name = "bert_question_answerer_demo",
    srcs = ["bert_question_answerer_demo.py"],
    data = ["//tensorflow_lite_support/examples/task/text/desktop:bert_question_answerer_demo"],
    deps = [
        # build rule placeholder: tensorflow dep,
        "@absl_py//absl:app",
        "@absl_py//absl/flags",
    ],
)
