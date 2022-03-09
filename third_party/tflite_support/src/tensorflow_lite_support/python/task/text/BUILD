# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "text_embedder",
    srcs = [
        "text_embedder.py",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/python/task/core:task_options",
        "//tensorflow_lite_support/python/task/core:task_utils",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embeddings_pb2",
        "//tensorflow_lite_support/python/task/text/pybinds:_pywrap_text_embedder",
        "//tensorflow_lite_support/python/task/text/pybinds:text_embedder_options_pb2",
    ],
)
