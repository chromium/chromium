# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:users"],
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
