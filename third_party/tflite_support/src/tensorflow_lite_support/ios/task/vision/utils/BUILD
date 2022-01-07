package(
    default_visibility = ["//tensorflow_lite_support:__subpackages__"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "GMLImageUtils",
    srcs = [
        "sources/GMLImageUtils.m",
    ],
    hdrs = [
        "sources/GMLImageUtils.h",
    ],
    module_name = "GMLImageUtils",
    sdk_frameworks = ["Accelerate"],
    deps = [
        "//tensorflow_lite_support/c/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/ios:TFLCommon",
        "//tensorflow_lite_support/ios:TFLCommonUtils",
        "//tensorflow_lite_support/odml/ios/image:MLImage",
    ],
)
