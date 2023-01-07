# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "optional_dependencies",
    srcs = [
        "optional_dependencies.py",
    ],
)

py_library(
    name = "base_options",
    srcs = ["base_options.py"],
    deps = [
        ":optional_dependencies",
        "//tensorflow_lite_support/python/task/core/proto:base_options_py_pb2",
    ],
)
