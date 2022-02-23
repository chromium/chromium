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
    deps = [
        "//tensorflow_lite_support/python/task/core:task_options",
        "//tensorflow_lite_support/python/task/processor/proto:bounding_box_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embedding_options_pb2",
        "//tensorflow_lite_support/python/task/processor/proto:embeddings_pb2",
        "//tensorflow_lite_support/python/task/vision:image_embedder",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/test:test_util",
        "//testing/pybase",
        "@absl_py//absl/testing:parameterized",
        "@pybind11_abseil//pybind11_abseil:status",
    ],
)
