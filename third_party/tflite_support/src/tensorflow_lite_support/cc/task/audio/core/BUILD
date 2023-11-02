package(
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "audio_buffer",
    hdrs = [
        "audio_buffer.h",
    ],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:statusor",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ],
)
