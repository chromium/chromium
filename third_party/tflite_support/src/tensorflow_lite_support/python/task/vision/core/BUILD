# Placeholder for internal Python strict test compatibility macro.
# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "color_space_type",
    srcs = ["color_space_type.py"],
)

py_library(
    name = "tensor_image",
    srcs = ["tensor_image.py"],
    deps = [
        ":color_space_type",
        # build rule placeholder: numpy dep,
        "//tensorflow_lite_support/python/task/vision/core/pybinds:image_utils",
    ],
)

py_test(
    name = "tensor_image_test",
    srcs = ["tensor_image_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
    ],
    deps = [
        ":color_space_type",
        ":tensor_image",
        # build rule placeholder: numpy dep,
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/vision/core/pybinds:image_utils",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)
