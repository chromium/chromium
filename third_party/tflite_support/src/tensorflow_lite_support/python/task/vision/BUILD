# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "image_embedder",
    srcs = [
        "image_embedder.py",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:bounding_box_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_pb2",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/task/vision/core/pybinds:image_utils",
        "//tensorflow_lite_support/python/task/vision/pybinds:_pywrap_image_embedder",
    ],
)

py_library(
    name = "image_classifier",
    srcs = [
        "image_classifier.py",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:bounding_box_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classification_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classifications_pb2",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/task/vision/core/pybinds:image_utils",
        "//tensorflow_lite_support/python/task/vision/pybinds:_pywrap_image_classifier",
    ],
)

py_library(
    name = "image_segmenter",
    srcs = [
        "image_segmenter.py",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:segmentation_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:segmentations_pb2",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/task/vision/core/pybinds:image_utils",
        "//tensorflow_lite_support/python/task/vision/pybinds:_pywrap_image_segmenter",
    ],
)

py_library(
    name = "image_searcher",
    srcs = [
        "image_searcher.py",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:bounding_box_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:search_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:search_result_pb2",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/task/vision/core/pybinds:image_utils",
        "//tensorflow_lite_support/python/task/vision/pybinds:_pywrap_image_searcher",
    ],
)

py_library(
    name = "object_detector",
    srcs = [
        "object_detector.py",
    ],
    deps = [
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:detection_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:detections_pb2",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/task/vision/core/pybinds:image_utils",
        "//tensorflow_lite_support/python/task/vision/pybinds:_pywrap_object_detector",
    ],
)
