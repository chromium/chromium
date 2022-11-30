licenses(["notice"])

package(
    default_visibility = [
        "//tensorflow_text:__subpackages__",
    ],
)

py_library(
    name = "wordpiece_vocab",
    srcs = ["__init__.py"],
    srcs_version = "PY3",
    deps = [
        ":bert_vocab_from_dataset",
        ":wordpiece_tokenizer_learner_lib",
    ],
)

py_library(
    name = "wordpiece_tokenizer_learner_lib",
    srcs = ["wordpiece_tokenizer_learner_lib.py"],
    srcs_version = "PY3",
    deps = [
        #  numpy dep,
    ],
)

py_test(
    name = "wordpiece_tokenizer_learner_test",
    srcs = ["wordpiece_tokenizer_learner_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":wordpiece_tokenizer_learner_lib",
        #  numpy dep,
        # python/data/ops:dataset_ops tensorflow dep,
        "//tensorflow_text:ops",
    ],
)

py_library(
    name = "bert_vocab_from_dataset",
    srcs = ["bert_vocab_from_dataset.py"],
    srcs_version = "PY3",
    deps = [
        ":wordpiece_tokenizer_learner_lib",
        "//tensorflow_text:bert_tokenizer",
    ],
)

py_test(
    name = "bert_vocab_from_dataset_test",
    srcs = ["bert_vocab_from_dataset_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":bert_vocab_from_dataset",
        ":wordpiece_tokenizer_learner_lib",
        "@absl_py//absl/testing:absltest",
        #  numpy dep,
        # tensorflow package dep,
    ],
)
