package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLAudioClassifier",
    srcs = [
        "sources/TFLAudioClassifier.m",
    ],
    hdrs = [
        "sources/TFLAudioClassifier.h",
    ],
    features = ["-layering_check"],
    module_name = "TFLAudioClassifier",
    deps = [
        "//tensorflow_lite_support/c/task/audio:audio_classifier",
        "//tensorflow_lite_support/ios:TFLCommonUtils",
        "//tensorflow_lite_support/ios/task/audio/core/audio_tensor:TFLAudioTensor",
        "//tensorflow_lite_support/ios/task/audio/core/audio_tensor/utils:TFLAudioTensorUtils",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptions",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationOptions",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationResult",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationResultHelpers",
    ],
)
