package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLFloatBuffer",
    srcs = [
        "sources/TFLFloatBuffer.m",
    ],
    hdrs = [
        "sources/TFLFloatBuffer.h",
    ],
    module_name = "TFLFloatBuffer",
)

objc_library(
    name = "TFLRingBuffer",
    srcs = [
        "sources/TFLRingBuffer.m",
    ],
    hdrs = [
        "sources/TFLRingBuffer.h",
    ],
    module_name = "TFLRingBuffer",
    deps = [
        ":TFLFloatBuffer",
        "//tensorflow_lite_support/ios:TFLCommon",
        "//tensorflow_lite_support/ios:TFLCommonUtils",
    ],
)

objc_library(
    name = "TFLAudioFormat",
    srcs = [
        "sources/TFLAudioFormat.m",
    ],
    hdrs = [
        "sources/TFLAudioFormat.h",
    ],
    module_name = "TFLAudioFormat",
)
