# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "audio_record",
    srcs = ["audio_record.py"],
)

py_library(
    name = "tensor_audio",
    srcs = ["tensor_audio.py"],
    deps = [
        ":audio_record",
        # build rule placeholder: numpy dep,
        "//tensorflow_lite_support/python/task/audio/core/pybinds:_pywrap_audio_buffer",
    ],
)
