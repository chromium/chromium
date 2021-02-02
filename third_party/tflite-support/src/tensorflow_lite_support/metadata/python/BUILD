load("//tensorflow_lite_support/metadata:build_defs.bzl", "stamp_metadata_parser_version")

package(
    default_visibility = [
        "//visibility:public",
    ],
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
    srcs_version = "PY2AND3",
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/custom_ops:expect_numpy_installed",
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata:schema_py",
        "//tensorflow_lite_support/metadata/cc/python:_pywrap_metadata_version",
        "//tensorflow_lite_support/metadata/flatbuffers_lib:_pywrap_flatbuffers",
        "//tensorflow_lite_support/tools:expect_flatbuffers_installed",
    ],
)
