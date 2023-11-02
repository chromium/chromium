package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLAudioTensorUtils",
    srcs = [
        "sources/TFLAudioTensor+Utils.m",
    ],
    hdrs = [
        "sources/TFLAudioTensor+Utils.h",
    ],
    module_name = "TFLAudioTensorUtils",
    deps = [
        "//tensorflow_lite_support/c/task/audio/core:audio_buffer",
        "//tensorflow_lite_support/ios/task/audio/core/audio_tensor:TFLAudioTensor",
    ],
)
