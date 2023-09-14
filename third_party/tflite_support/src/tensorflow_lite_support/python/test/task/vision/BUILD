# Placeholder for internal Python strict test compatibility macro.

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

py_test(
    name = "image_embedder_test",
    srcs = ["image_embedder_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tags = ["not_run:arm"],
    deps = [
        # build rule placeholder: numpy dep,
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:bounding_box_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_pb2",
        "//tensorflow_lite_support/python/task/vision:image_embedder",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "image_classifier_test",
    srcs = ["image_classifier_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    deps = [
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:bounding_box_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:class_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classification_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:classifications_pb2",
        "//tensorflow_lite_support/python/task/vision:image_classifier",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "image_segmenter_test",
    srcs = ["image_segmenter_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    deps = [
        # build rule placeholder: numpy dep,
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:segmentation_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:segmentations_pb2",
        "//tensorflow_lite_support/python/task/vision:image_segmenter",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "image_searcher_test",
    srcs = ["image_searcher_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_indices",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tags = ["not_run:arm"],
    deps = [
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:bounding_box_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:search_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:search_result_pb2",
        "//tensorflow_lite_support/python/task/vision:image_searcher",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)

py_test(
    name = "object_detector_test",
    srcs = ["object_detector_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tags = ["not_run:arm"],
    deps = [
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/core:base_options",
        "//tensorflow_lite_support/python/task/processor/proto:bounding_box_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:class_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:detection_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:detections_pb2",
        "//tensorflow_lite_support/python/task/vision:object_detector",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)
