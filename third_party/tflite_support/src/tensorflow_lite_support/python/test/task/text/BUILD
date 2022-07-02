# Placeholder for internal Python strict test compatibility macro.

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

py_test(
    name = "text_embedder_test",
    srcs = ["text_embedder_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobilebert_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:regex_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:universal_sentence_encoder_qa",
    ],
    deps = [
        # build rule placeholder: numpy dep,
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core/proto:base_options_py_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/text:text_embedder",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "text_searcher_test",
    srcs = ["text_searcher_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobilebert_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:regex_embedding_with_metadata",
        "//tensorflow_lite_support/cc/test/testdata/task/text:test_indices",
        "//tensorflow_lite_support/cc/test/testdata/task/text:test_searchers",
        "//tensorflow_lite_support/cc/test/testdata/task/text:universal_sentence_encoder_qa",
    ],
    deps = [
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core/proto:base_options_py_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:search_options_py_pb2",
        "//tensorflow_lite_support/python/task/text:text_searcher",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)
