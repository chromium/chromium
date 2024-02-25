load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

pybind_extension(
    name = "image_utils",
    srcs = [
        "image_utils.cc",
    ],
    module_name = "image_utils",
    deps = [
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_abseil//pybind11_abseil:status_casters",
    ],
)
