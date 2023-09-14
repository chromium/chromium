package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLAudioRecord",
    srcs = [
        "sources/TFLAudioRecord.m",
    ],
    hdrs = [
        "sources/TFLAudioRecord.h",
    ],
    module_name = "TFLAudioRecord",
    deps = [
        "//tensorflow_lite_support/ios:TFLCommonUtils",
        "//tensorflow_lite_support/ios/task/audio/core:TFLAudioFormat",
        "//tensorflow_lite_support/ios/task/audio/core:TFLFloatBuffer",
        "//tensorflow_lite_support/ios/task/audio/core:TFLRingBuffer",
    ],
)
