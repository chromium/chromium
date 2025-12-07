load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

pybind_extension(
    name = "_pywrap_audio_buffer",
    srcs = [
        "_pywrap_audio_buffer.cc",
    ],
    enable_stub_generation = True,
    module_name = "_pywrap_audio_buffer",
    pytype_srcs = [
        "_pywrap_audio_buffer.pyi",
    ],
    deps = [
        "//tensorflow_lite_support/cc/task/audio/core:audio_buffer",
        "//tensorflow_lite_support/cc/task/audio/utils:audio_utils",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
    ],
)
