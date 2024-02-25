# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],
)

py_library(
    name = "metadata_writer",
    srcs = [
        "metadata_writer.py",
    ],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        ":metadata_info",
        ":writer_utils",
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata:schema_py",
        "//tensorflow_lite_support/metadata/python:metadata",
        "@flatbuffers//:runtime_py",
    ],
)

py_library(
    name = "metadata_info",
    srcs = [
        "metadata_info.py",
    ],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        ":writer_utils",
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata:schema_py",
    ],
)

py_library(
    name = "writer_utils",
    srcs = [
        "writer_utils.py",
    ],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata:schema_py",
    ],
)

py_library(
    name = "image_classifier",
    srcs = [
        "image_classifier.py",
    ],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        ":metadata_info",
        ":metadata_writer",
        ":writer_utils",
        "//tensorflow_lite_support/metadata:metadata_schema_py",
    ],
)

py_library(
    name = "object_detector",
    srcs = [
        "object_detector.py",
    ],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        ":metadata_info",
        ":metadata_writer",
        ":writer_utils",
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata:schema_py",
        "//tensorflow_lite_support/metadata/python:metadata",
        "@flatbuffers//:runtime_py",
    ],
)

py_library(
    name = "image_segmenter",
    srcs = [
        "image_segmenter.py",
    ],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        ":metadata_info",
        ":metadata_writer",
        ":writer_utils",
        "//tensorflow_lite_support/metadata:metadata_schema_py",
    ],
)

py_library(
    name = "nl_classifier",
    srcs = [
        "nl_classifier.py",
    ],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        ":metadata_info",
        ":metadata_writer",
        ":writer_utils",
    ],
)

py_library(
    name = "audio_classifier",
    srcs = [
        "audio_classifier.py",
    ],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        ":metadata_info",
        ":metadata_writer",
        ":writer_utils",
    ],
)

py_library(
    name = "bert_nl_classifier",
    srcs = [
        "bert_nl_classifier.py",
    ],
    srcs_version = "PY3",
    visibility = ["//visibility:public"],
    deps = [
        ":metadata_info",
        ":metadata_writer",
        ":writer_utils",
    ],
)
