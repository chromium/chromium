package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "audio_buffer_cpp_c_utils",
    srcs = [
        "audio_buffer_cpp_c_utils.cc",
    ],
    hdrs = [
        "audio_buffer_cpp_c_utils.h",
    ],
    deps = [
        "//tensorflow_lite_support/c/task/audio/core:audio_buffer",
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/task/audio/core:audio_buffer",
        "@com_google_absl//absl/strings:str_format",
    ],
)
