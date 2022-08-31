# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "audio_embedder",
    srcs = [
        "audio_embedder.py",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/audio/core:audio_record",
        "//tensorflow_lite_support/python/task/audio/core:tensor_audio",
        "//tensorflow_lite_support/python/task/audio/core/pybinds:_pywrap_audio_buffer",
        "//tensorflow_lite_support/python/task/audio/pybinds:_pywrap_audio_embedder",
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_pb2",
    ],
)

py_library(
    name = "audio_classifier",
    srcs = [
        "audio_classifier.py",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/audio/core:audio_record",
        "//tensorflow_lite_support/python/task/audio/core:tensor_audio",
        "//tensorflow_lite_support/python/task/audio/core/pybinds:_pywrap_audio_buffer",
        "//tensorflow_lite_support/python/task/audio/pybinds:_pywrap_audio_classifier",
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:classification_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classifications_pb2",
    ],
)
