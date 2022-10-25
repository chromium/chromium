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
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptions",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationOptions",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationResult",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationResultHelpers",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageUtils",
        "//tensorflow_lite_support/odml/ios/image:MLImage",
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
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptions",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationOptions",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLDetectionResult",
        "//tensorflow_lite_support/ios/task/processor:TFLDetectionResultHelpers",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageUtils",
        "//tensorflow_lite_support/odml/ios/image:MLImage",
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
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptions",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLSegmentationResult",
        "//tensorflow_lite_support/ios/task/processor:TFLSegmentationResultHelpers",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageUtils",
        "//tensorflow_lite_support/odml/ios/image:MLImage",
    ],
)

objc_library(
    name = "TFLImageSearcher",
    srcs = [
        "sources/TFLImageSearcher.mm",
    ],
    hdrs = [
        "sources/TFLImageSearcher.h",
    ],
    copts = [
        "-ObjC++",
        "-std=c++17",
    ],
    features = ["-layering_check"],
    module_name = "TFLImageSearcher",
    deps = [
        "//tensorflow_lite_support/cc/task/vision:image_searcher",
        "//tensorflow_lite_support/ios:TFLCommonCppUtils",
        "//tensorflow_lite_support/ios:TFLCommonUtils",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptionsCppHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLEmbeddingOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLSearchOptionsHelpers",
        "//tensorflow_lite_support/ios/task/processor:TFLSearchResultHelpers",
        "//tensorflow_lite_support/ios/task/vision/utils:GMLImageCppUtils",
    ],
)
