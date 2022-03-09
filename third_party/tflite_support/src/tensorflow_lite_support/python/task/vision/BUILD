# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "image_embedder",
    srcs = [
        "image_embedder.py",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/core:task_options",
        "//tensorflow_lite_support/python/task/core/proto:configuration_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:bounding_box_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embeddings_pb2",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/task/vision/core/pybinds:image_utils",
        "//tensorflow_lite_support/python/task/vision/pybinds:_pywrap_image_embedder",
        "//tensorflow_lite_support/python/task/vision/pybinds:image_embedder_options_pb2",
    ],
)

py_library(
    name = "image_classifier",
    srcs = [
        "image_classifier.py",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/core:task_options",
        "//tensorflow_lite_support/python/task/core:task_utils",
        "//tensorflow_lite_support/python/task/processor/proto:bounding_box_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classification_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classifications_pb2",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/task/vision/core/pybinds:image_utils",
        "//tensorflow_lite_support/python/task/vision/pybinds:_pywrap_image_classifier",
        "//tensorflow_lite_support/python/task/vision/pybinds:image_classifier_options_pb2",
    ],
)

py_library(
    name = "object_detector",
    srcs = [
        "object_detector.py",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/core:task_options",
        "//tensorflow_lite_support/python/task/core:task_utils",
        "//tensorflow_lite_support/python/task/processor/proto:bounding_box_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:detection_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:detections_pb2",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/task/vision/core/pybinds:image_utils",
        "//tensorflow_lite_support/python/task/vision/pybinds:_pywrap_object_detector",
        "//tensorflow_lite_support/python/task/vision/pybinds:object_detector_options_pb2",
    ],
)
