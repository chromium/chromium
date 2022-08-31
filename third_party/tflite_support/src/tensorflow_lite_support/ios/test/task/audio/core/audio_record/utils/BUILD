package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "AVAudioPCMBufferUtils",
    srcs = [
        "sources/AVAudioPCMBuffer+Utils.m",
    ],
    hdrs = [
        "sources/AVAudioPCMBuffer+Utils.h",
    ],
    module_name = "AVAudioPCMBufferUtils",
    deps = [
        "//tensorflow_lite_support/ios/task/audio/core:TFLAudioFormat",
        "//tensorflow_lite_support/ios/task/audio/core:TFLFloatBuffer",
    ],
)
