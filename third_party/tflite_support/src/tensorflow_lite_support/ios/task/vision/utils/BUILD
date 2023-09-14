package(
    default_visibility = ["//tensorflow_lite_support:__subpackages__"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "GMLImageUtils",
    srcs = [
        "sources/GMLImage+Utils.m",
    ],
    hdrs = [
        "sources/GMLImage+Utils.h",
    ],
    features = ["-layering_check"],
    module_name = "GMLImageUtils",
    # tflite_support:oss-insert sdk_frameworks = ["CoreGraphics", "CoreVideo", "Accelerate", "CoreMedia"],
    deps = [
        "//tensorflow_lite_support/c/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/ios:TFLCommonUtils",
        "//tensorflow_lite_support/odml/ios/image:MLImage",
    ],
)

objc_library(
    name = "GMLImageCppUtils",
    srcs = [
        "sources/GMLImage+CppUtils.mm",
    ],
    hdrs = [
        "sources/GMLImage+CppUtils.h",
    ],
    features = ["-layering_check"],
    module_name = "GMLImageCppUtils",
    deps = [
        ":GMLImageUtils",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/ios:TFLCommonCppUtils",
        "//tensorflow_lite_support/odml/ios/image:MLImage",
        "@com_google_absl//absl/strings:str_format",
    ],
)
