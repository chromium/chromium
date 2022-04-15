# Placeholder for internal Python strict test compatibility macro.

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

py_test(
    name = "audio_embedder_test",
    srcs = ["audio_embedder_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/audio:test_audio_clips",
        "//tensorflow_lite_support/cc/test/testdata/task/audio:test_models",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/audio:audio_embedder",
        "//tensorflow_lite_support/python/task/audio/core:audio_record",
        "//tensorflow_lite_support/python/task/audio/core:tensor_audio",
        "//tensorflow_lite_support/python/task/core/proto:base_options_py_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/test:base_test",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "audio_classifier_test",
    srcs = ["audio_classifier_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/audio:test_audio_clips",
        "//tensorflow_lite_support/cc/test/testdata/task/audio:test_models",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/audio:audio_classifier",
        "//tensorflow_lite_support/python/task/audio/core:audio_record",
        "//tensorflow_lite_support/python/task/audio/core:tensor_audio",
        "//tensorflow_lite_support/python/task/core/proto:base_options_py_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:class_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classification_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classifications_pb2",
        "//tensorflow_lite_support/python/test:base_test",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
        "@com_google_protobuf//:protobuf_python",
    ],
)
