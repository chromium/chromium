package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLAudioTensor",
    srcs = [
        "sources/TFLAudioTensor.m",
    ],
    hdrs = [
        "sources/TFLAudioTensor.h",
    ],
    module_name = "TFLAudioTensor",
    deps = [
        "//tensorflow_lite_support/ios:TFLCommon",
        "//tensorflow_lite_support/ios:TFLCommonUtils",
        "//tensorflow_lite_support/ios/task/audio/core:TFLFloatBuffer",
        "//tensorflow_lite_support/ios/task/audio/core:TFLRingBuffer",
        "//tensorflow_lite_support/ios/task/audio/core/audio_record:TFLAudioRecord",
    ],
)
