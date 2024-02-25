# Placeholder for internal Python strict test compatibility macro.

package(
    licenses = ["notice"],  # Apache 2.0
)

py_test(
    name = "audio_record_test",
    srcs = ["audio_record_test.py"],
    deps = [
        # build rule placeholder: numpy dep,
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/audio/core:audio_record",
    ],
)

py_test(
    name = "tensor_audio_test",
    srcs = ["tensor_audio_test.py"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/audio:test_audio_clips",
    ],
    deps = [
        # build rule placeholder: numpy dep,
        # build rule placeholder: tensorflow dep,
        "//tensorflow_lite_support/python/task/audio/core:audio_record",
        "//tensorflow_lite_support/python/task/audio/core:tensor_audio",
        "//tensorflow_lite_support/python/task/audio/core/pybinds:_pywrap_audio_buffer",
        "//tensorflow_lite_support/python/test:test_util",
        "@absl_py//absl/testing:parameterized",
    ],
)
