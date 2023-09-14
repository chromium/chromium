# Description: some utilitiy function for audio processing.

package(
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "audio_utils",
    srcs = [
        "audio_utils.cc",
    ],
    hdrs = ["audio_utils.h"],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        ":wav_io",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/audio/core:audio_buffer",
    ],
)

cc_library(
    name = "wav_io",
    srcs = [
        "wav_io.cc",
    ],
    hdrs = ["wav_io.h"],
    visibility = ["//tensorflow_lite_support:internal"],
    deps = [
        "//tensorflow_lite_support/cc/port:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)
