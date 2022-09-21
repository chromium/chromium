# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "embedding_pb2",
    srcs = ["embedding_pb2.py"],
    deps = [
        # build rule placeholder: numpy dep,
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "embedding_options_pb2",
    srcs = ["embedding_options_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "qa_answers_pb2",
    srcs = ["qa_answers_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:qa_answers_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "bounding_box_pb2",
    srcs = ["bounding_box_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:bounding_box_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "class_pb2",
    srcs = ["class_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:class_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "classifications_pb2",
    srcs = ["classifications_pb2.py"],
    deps = [
        ":class_pb2",
        "//tensorflow_lite_support/cc/task/processor/proto:classifications_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "classification_options_pb2",
    srcs = ["classification_options_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:classification_options_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "detections_pb2",
    srcs = ["detections_pb2.py"],
    deps = [
        ":bounding_box_pb2",
        ":class_pb2",
        "//tensorflow_lite_support/cc/task/processor/proto:detections_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "detection_options_pb2",
    srcs = ["detection_options_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:detection_options_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "segmentations_pb2",
    srcs = ["segmentations_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/vision/proto:segmentations_py_pb2",
        # build rule placeholder: numpy dep,
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "segmentation_options_pb2",
    srcs = ["segmentation_options_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:segmentation_options_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "search_options_pb2",
    srcs = ["search_options_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:search_options_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "search_result_pb2",
    srcs = ["search_result_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:search_result_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "clu_annotation_options_pb2",
    srcs = ["clu_annotation_options_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:clu_annotation_options_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)

py_library(
    name = "clu_pb2",
    srcs = ["clu_pb2.py"],
    deps = [
        ":class_pb2",
        "//tensorflow_lite_support/cc/task/processor/proto:clu_py_pb2",
        "//tensorflow_lite_support/python/task/core:optional_dependencies",
    ],
)
