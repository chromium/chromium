# Description:
#  Tools for building the TensorFlow pip package.

package(default_visibility = ["//visibility:private"])

COMMON_PIP_DEPS = [
    ":licenses",
    "MANIFEST.in",
    "README",
    "setup.py",
    "//tensorflow_lite_support/codegen/python:codegen",
    "//tensorflow_lite_support/metadata/python:metadata",
    "//tensorflow_lite_support/metadata/python/metadata_writers:writer_utils",
    "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_info",
    "//tensorflow_lite_support/metadata/python/metadata_writers:image_classifier",
    "//tensorflow_lite_support/metadata/python/metadata_writers:object_detector",
    "//tensorflow_lite_support/metadata/python/metadata_writers:image_segmenter",
    "//tensorflow_lite_support/metadata/python/metadata_writers:audio_classifier",
    "//tensorflow_lite_support/metadata/python/metadata_writers:nl_classifier",
    "//tensorflow_lite_support/metadata/python/metadata_writers:bert_nl_classifier",
]

TASK_PIP_DEPS = [
    "//tensorflow_lite_support/python/task/vision:image_classifier",
    "//tensorflow_lite_support/python/task/vision:image_embedder",
    "//tensorflow_lite_support/python/task/vision:image_segmenter",
    "//tensorflow_lite_support/python/task/vision:image_searcher",
    "//tensorflow_lite_support/python/task/vision:object_detector",
    "//tensorflow_lite_support/python/task/text:text_embedder",
    "//tensorflow_lite_support/python/task/text:text_searcher",
    "//tensorflow_lite_support/python/task/text:nl_classifier",
    "//tensorflow_lite_support/python/task/text:bert_nl_classifier",
    "//tensorflow_lite_support/python/task/text:bert_question_answerer",
    "//tensorflow_lite_support/python/task/text:bert_clu_annotator",
    "//tensorflow_lite_support/python/task/audio:audio_classifier",
    "//tensorflow_lite_support/python/task/audio:audio_embedder",
    # For Model Maker Searcher API to build ScaNN index.
    "//tensorflow_lite_support/scann_ondevice/cc/python:index_builder",
    "//tensorflow_lite_support/scann_ondevice/cc/core:serialized_searcher_py_pb2",
    "//tensorflow_lite_support/scann_ondevice/cc/test/python:leveldb_testing_utils",
]

filegroup(
    name = "licenses",
    data = [
        "//:LICENSE",
        "@org_tensorflow//:LICENSE",
    ],
)

sh_binary(
    name = "build_pip_package",
    srcs = ["build_pip_package.sh"],
    data = COMMON_PIP_DEPS +
           select({
               "@org_tensorflow//tensorflow:windows": [
                   ":simple_console_for_windows",
               ],
               # Task Library is not supported on Windows yet.
               "//conditions:default": TASK_PIP_DEPS,
           }),
)

# On Windows, python binary is a zip file of runfiles tree.
# Add everything to its data dependency for generating a runfiles tree
# for building the pip package on Windows.
py_binary(
    name = "simple_console_for_windows",
    srcs = ["simple_console_for_windows.py"],
    data = COMMON_PIP_DEPS,
    srcs_version = "PY2AND3",
    deps = [],
)
