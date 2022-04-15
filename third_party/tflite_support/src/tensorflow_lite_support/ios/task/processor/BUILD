package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLClassificationOptions",
    srcs = [
        "sources/TFLClassificationOptions.m",
    ],
    hdrs = [
        "sources/TFLClassificationOptions.h",
    ],
    module_name = "TFLClassificationOptions",
)

objc_library(
    name = "TFLCategory",
    srcs = [
        "sources/TFLCategory.m",
    ],
    hdrs = [
        "sources/TFLCategory.h",
    ],
    module_name = "TFLCategory",
)

objc_library(
    name = "TFLClassificationResult",
    srcs = [
        "sources/TFLClassificationResult.m",
    ],
    hdrs = [
        "sources/TFLClassificationResult.h",
    ],
    module_name = "TFLClassificationResult",
    deps = [
        ":TFLCategory",
    ],
)

objc_library(
    name = "TFLDetectionResult",
    srcs = [
        "sources/TFLDetectionResult.m",
    ],
    hdrs = [
        "sources/TFLDetectionResult.h",
    ],
    module_name = "TFLDetectionResult",
    deps = [
        ":TFLCategory",
    ],
)

objc_library(
    name = "TFLSegmentationResult",
    srcs = [
        "sources/TFLSegmentationResult.m",
    ],
    hdrs = [
        "sources/TFLSegmentationResult.h",
    ],
    module_name = "TFLSegmentationResult",
)

objc_library(
    name = "TFLClassificationOptionsHelpers",
    srcs = [
        "sources/TFLClassificationOptions+Helpers.m",
    ],
    hdrs = [
        "sources/TFLClassificationOptions+Helpers.h",
    ],
    module_name = "TFLClassificationOptionsHelpers",
    deps = [
        "//tensorflow_lite_support/c/task/processor:classification_options",
        "//tensorflow_lite_support/ios:TFLCommon",
        "//tensorflow_lite_support/ios:TFLCommonUtils",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationOptions",
    ],
)

objc_library(
    name = "TFLCategoryHelpers",
    srcs = [
        "sources/TFLCategory+Helpers.m",
    ],
    hdrs = [
        "sources/TFLCategory+Helpers.h",
    ],
    module_name = "TFLCategoryHelpers",
    deps = [
        "//tensorflow_lite_support/c/task/processor:category",
        "//tensorflow_lite_support/ios/task/processor:TFLCategory",
    ],
)

objc_library(
    name = "TFLClassificationResultHelpers",
    srcs = [
        "sources/TFLClassificationResult+Helpers.m",
    ],
    hdrs = [
        "sources/TFLClassificationResult+Helpers.h",
    ],
    module_name = "TFLClassificationResultHelpers",
    deps = [
        ":TFLCategoryHelpers",
        "//tensorflow_lite_support/c/task/processor:classification_result",
        "//tensorflow_lite_support/ios/task/processor:TFLClassificationResult",
    ],
)

objc_library(
    name = "TFLDetectionResultHelpers",
    srcs = [
        "sources/TFLDetectionResult+Helpers.m",
    ],
    hdrs = [
        "sources/TFLDetectionResult+Helpers.h",
    ],
    module_name = "TFLDetectionResultHelpers",
    deps = [
        ":TFLCategoryHelpers",
        "//tensorflow_lite_support/c/task/processor:detection_result",
        "//tensorflow_lite_support/ios/task/processor:TFLDetectionResult",
    ],
)

objc_library(
    name = "TFLSegmentationResultHelpers",
    srcs = [
        "sources/TFLSegmentationResult+Helpers.m",
    ],
    hdrs = [
        "sources/TFLSegmentationResult+Helpers.h",
    ],
    module_name = "TFLSegmentationResultHelpers",
    deps = [
        "//tensorflow_lite_support/c/task/processor:segmentation_result",
        "//tensorflow_lite_support/ios/task/processor:TFLSegmentationResult",
    ],
)
