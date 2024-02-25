# Placeholder: load py_library
# Placeholder: load py_binary
load("//tensorflow_lite_support/metadata:build_defs.bzl", "stamp_metadata_parser_version")
# Placeholder for internal Python strict library compatibility macro.

package(
    licenses = ["notice"],  # Apache 2.0
)

stamp_metadata_parser_version(
    name = "metadata_parser_py",
    srcs = ["metadata_parser.py.template"],
    outs = ["metadata_parser.py"],
)

py_library(
    name = "metadata",
    srcs = [
        "metadata.py",
        ":metadata_parser_py",
    ],
    data = ["//tensorflow_lite_support/metadata:metadata_schema.fbs"],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        # build rule placeholder: numpy dep,
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata:schema_py",
        "//tensorflow_lite_support/metadata/cc/python:_pywrap_metadata_version",
        "//tensorflow_lite_support/metadata/flatbuffers_lib:_pywrap_flatbuffers",
        "@flatbuffers//:runtime_py",
    ],
)

py_binary(
    name = "metadata_displayer",
    srcs = ["metadata_displayer.py"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":metadata",
        "@absl_py//absl:app",
        "@absl_py//absl/flags",
    ],
)

py_library(
    name = "metadata_writer_for_task",
    srcs = ["metadata_writer_for_task.py"],
    srcs_version = "PY3",
    visibility = [
        "//tensorflow_lite_support:users",
    ],
    deps = [
        ":metadata",
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_info",
        "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_writer",
        "//tensorflow_lite_support/metadata/python/metadata_writers:writer_utils",
    ],
)
