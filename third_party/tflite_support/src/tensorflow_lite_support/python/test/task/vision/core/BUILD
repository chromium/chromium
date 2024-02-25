# Placeholder for internal Python strict test compatibility macro.

package(
    licenses = ["notice"],  # Apache 2.0
)

py_test(
    name = "tensor_image_test",
    srcs = ["tensor_image_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
    ],
    deps = [
        # build rule placeholder: numpy dep,
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/vision/core:color_space_type",
        "//tensorflow_lite_support/python/task/vision/core:tensor_image",
        "//tensorflow_lite_support/python/task/vision/core/pybinds:image_utils",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)
