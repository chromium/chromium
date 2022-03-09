# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "task_options",
    srcs = ["task_options.py"],
)

py_library(
    name = "task_utils",
    srcs = ["task_utils.py"],
    deps = [
        ":task_options",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_py_pb2",
        "//tensorflow_lite_support/python/task/core/proto:configuration_pb2",
    ],
)
