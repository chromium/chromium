package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLImageClassifier",
    srcs = [
        "sources/TFLImageClassifier.m",
    ],
    hdrs = [
        "sources/TFLImageClassifier.h",
    ],
    features = ["-layering_check"],
    module_name = "TFLImageClassifier",
    deps = [
        "//tensorflow_lite_support/c/task/vision:image_classifier",
        "//tensorflow_lite_support/ios:TFLCommonUtils",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationResultHelpers",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageUtils",
    ],
)

objc_library(
    name = "TFLObjectDetector",
    srcs = [
        "sources/TFLObjectDetector.m",
    ],
    hdrs = [
        "sources/TFLObjectDetector.h",
    ],
    features = ["-layering_check"],
    module_name = "TFLObjectDetector",
    deps = [
        "//tensorflow_lite_support/c/task/vision:object_detector",
        "//tensorflow_lite_support/ios:TFLCommonUtils",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLDetectionResultHelpers",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageUtils",
    ],
)

objc_library(
    name = "TFLImageSegmenter",
    srcs = [
        "sources/TFLImageSegmenter.m",
    ],
    hdrs = [
        "sources/TFLImageSegmenter.h",
    ],
    features = ["-layering_check"],
    module_name = "TFLImageSegmenter",
    deps = [
        "//tensorflow_lite_support/c/task/vision:image_segmenter",
        "//tensorflow_lite_support/ios:TFLCommonUtils",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLSegmentationResultHelpers",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageUtils",
    ],
)
