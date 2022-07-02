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
        "//tensorflow_lite_support/python/task/core/proto:base_options_py_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_pb2",
        "//tensorflow_lite_support/python/task/text/pybinds:_pywrap_text_embedder",
    ],
)

py_library(
    name = "text_searcher",
    srcs = [
        "text_searcher.py",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/python/task/core/proto:base_options_py_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:search_options_py_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:search_result_pb2",
        "//tensorflow_lite_support/python/task/text/pybinds:_pywrap_text_searcher",
    ],
)
