# Placeholder for internal Python strict test compatibility macro.
# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "test_utils",
    testonly = 1,
    srcs = [
        "test_utils.py",
    ],
    visibility = ["//tensorflow_lite_support/metadata/python/tests:__subpackages__"],
)

py_test(
    name = "metadata_writer_test",
    srcs = ["metadata_writer_test.py"],
    data = [
        "//tensorflow_lite_support/metadata/python/tests/testdata:test_files",
        "//tensorflow_lite_support/metadata/python/tests/testdata/audio_classifier:test_files",
        "//tensorflow_lite_support/metadata/python/tests/testdata/question_answerer:test_files",
    ],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":test_utils",
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata/python:metadata",
        "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_info",
        "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_writer",
        "@flatbuffers//:runtime_py",
    ],
)

py_test(
    name = "metadata_info_test",
    srcs = ["metadata_info_test.py"],
    data = [
        "//tensorflow_lite_support/metadata/python/tests/testdata:test_files",
        "//tensorflow_lite_support/metadata/python/tests/testdata/bert_nl_classifier:test_files",
        "//tensorflow_lite_support/metadata/python/tests/testdata/image_classifier:test_files",
    ],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":test_utils",
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata:schema_py",
        "//tensorflow_lite_support/metadata/python:metadata",
        "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_info",
        "@absl_py//absl/testing:parameterized",
        "@flatbuffers//:runtime_py",
    ],
)

py_test(
    name = "writer_utils_test",
    srcs = ["writer_utils_test.py"],
    data = ["//tensorflow_lite_support/metadata/python/tests/testdata/object_detector:test_files"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":test_utils",
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/metadata:schema_py",
        "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_info",
        "//tensorflow_lite_support/metadata/python/metadata_writers:writer_utils",
    ],
)

py_test(
    name = "image_classifier_test",
    srcs = ["image_classifier_test.py"],
    data = ["//tensorflow_lite_support/metadata/python/tests/testdata/image_classifier:test_files"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":test_utils",
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata/python/metadata_writers:image_classifier",
        "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_info",
        "@absl_py//absl/testing:parameterized",
        "@flatbuffers//:runtime_py",
    ],
)

py_test(
    name = "object_detector_test",
    srcs = ["object_detector_test.py"],
    data = ["//tensorflow_lite_support/metadata/python/tests/testdata/object_detector:test_files"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":test_utils",
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata/python:metadata",
        "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_info",
        "//tensorflow_lite_support/metadata/python/metadata_writers:object_detector",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "image_segmenter_test",
    srcs = ["image_segmenter_test.py"],
    data = ["//tensorflow_lite_support/metadata/python/tests/testdata/image_segmenter:test_files"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":test_utils",
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/metadata/python/metadata_writers:image_segmenter",
        "@flatbuffers//:runtime_py",
    ],
)

py_test(
    name = "nl_classifier_test",
    srcs = ["nl_classifier_test.py"],
    data = ["//tensorflow_lite_support/metadata/python/tests/testdata/nl_classifier:test_files"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":test_utils",
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/metadata/python:metadata",
        "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_info",
        "//tensorflow_lite_support/metadata/python/metadata_writers:nl_classifier",
        "@flatbuffers//:runtime_py",
    ],
)

py_test(
    name = "audio_classifier_test",
    srcs = ["audio_classifier_test.py"],
    data = ["//tensorflow_lite_support/metadata/python/tests/testdata/audio_classifier:test_files"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":test_utils",
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/metadata:metadata_schema_py",
        "//tensorflow_lite_support/metadata/python/metadata_writers:audio_classifier",
        "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_info",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "bert_nl_classifier_test",
    srcs = ["bert_nl_classifier_test.py"],
    data = ["//tensorflow_lite_support/metadata/python/tests/testdata/bert_nl_classifier:test_files"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":test_utils",
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/metadata/python:metadata",
        "//tensorflow_lite_support/metadata/python/metadata_writers:bert_nl_classifier",
        "//tensorflow_lite_support/metadata/python/metadata_writers:metadata_info",
    ],
)
