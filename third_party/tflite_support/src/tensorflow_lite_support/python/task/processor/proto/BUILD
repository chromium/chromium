# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "embeddings_pb2",
    srcs = ["embeddings_pb2.py"],
    deps = ["//tensorflow_lite_support/cc/task/vision/proto:embeddings_py_pb2"],
)

py_library(
    name = "embedding_options_pb2",
    srcs = ["embedding_options_pb2.py"],
    deps = ["//tensorflow_lite_support/cc/task/processor/proto:embedding_options_py_pb2"],
)

py_library(
    name = "bounding_box_pb2",
    srcs = ["bounding_box_pb2.py"],
    deps = ["//tensorflow_lite_support/cc/task/vision/proto:bounding_box_py_pb2"],
)

py_library(
    name = "class_pb2",
    srcs = ["class_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/vision/proto:class_py_pb2",
    ],
)

py_library(
    name = "classifications_pb2",
    srcs = ["classifications_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/vision/proto:classifications_py_pb2",
    ],
)

py_library(
    name = "classification_options_pb2",
    srcs = ["classification_options_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:classification_options_py_pb2",
    ],
)

py_library(
    name = "detections_pb2",
    srcs = ["detections_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/vision/proto:detections_py_pb2",
    ],
)

py_library(
    name = "detection_options_pb2",
    srcs = ["detection_options_pb2.py"],
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:detection_options_py_pb2",
    ],
)
