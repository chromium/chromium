load("//tensorflow_lite_support/tools/build_rules:http_files.bzl", "tflite_file", "tflite_model")

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "test_model_nl_classifier_with_regex_tokenizer.tflite",
])

filegroup(
    name = "nl_classifier_models",
    srcs = glob([
        "test_model_nl_classifier*.tflite",
    ]),
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
)

filegroup(
    name = "bert_nl_classifier_models",
    srcs = [
        ":bert_nl_classifier",
    ],
)

filegroup(
    name = "mobile_bert_model",
    srcs = [
        ":mobilebert_float",
        ":mobilebert_vocab",
        ":mobilebert_with_metadata",
    ],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
)

filegroup(
    name = "albert_model",
    srcs = [
        ":30k-clean",
        ":albert",
        ":albert_with_metadata",
    ],
)

filegroup(
    name = "regex_tokenizer_files",
    srcs = [
        "empty_vocab_for_regex_tokenizer.txt",
        "vocab_for_regex_tokenizer.txt",
    ],
)

filegroup(
    name = "universal_sentence_encoder_qa",
    data = [":universal_sentence_encoder_qa_with_metadata"],
)

tflite_model(name = "mobilebert_embedding_with_metadata")

filegroup(
    name = "regex_embedding_with_metadata",
    srcs = [
        "regex_one_embedding_with_metadata.tflite",
        "regex_two_embeddings_with_metadata.tflite",
    ],
)

tflite_model(name = "bert_nl_classifier")

tflite_model(name = "albert")

tflite_model(name = "albert_with_metadata")

tflite_model(name = "universal_sentence_encoder_qa_with_metadata")

tflite_model(name = "mobilebert_float")

tflite_file(
    name = "mobilebert_vocab",
    extension = "txt",
)

tflite_model(name = "mobilebert_with_metadata")

tflite_file(
    name = "30k-clean",
    extension = "model",
)

filegroup(
    name = "test_indices",
    srcs = glob([
        "*.ldb",
    ]),
)

filegroup(
    name = "test_searchers",
    srcs = [
        "mobilebert_searcher.tflite",
        "regex_searcher.tflite",
        "universal_sentence_encoder_searcher.tflite",
    ],
)
