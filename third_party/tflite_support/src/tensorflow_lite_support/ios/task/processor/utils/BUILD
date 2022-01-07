package(
    default_visibility = ["//tensorflow_lite_support:__subpackages__"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLClassificationUtils",
    srcs = [
        "sources/TFLClassificationUtils.m",
    ],
    hdrs = [
        "sources/TFLClassificationUtils.h",
    ],
    module_name = "TFLClassificationUtils",
    deps = [
        "//tensorflow_lite_support/c/task/processor:classification_result",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationResult",
    ],
)
