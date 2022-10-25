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
    deps = [
        "//tensorflow_lite_support/ios:TFLCommonUtils",
    ],
)

objc_library(
    name = "TFLSearchResult",
    srcs = [
        "sources/TFLSearchResult.m",
    ],
    hdrs = [
        "sources/TFLSearchResult.h",
    ],
    module_name = "TFLSearchResult",
)

objc_library(
    name = "TFLEmbeddingOptions",
    srcs = [
        "sources/TFLEmbeddingOptions.m",
    ],
    hdrs = [
        "sources/TFLEmbeddingOptions.h",
    ],
    module_name = "TFLEmbeddingOptions",
)

objc_library(
    name = "TFLSearchOptions",
    srcs = [
        "sources/TFLSearchOptions.m",
    ],
    hdrs = [
        "sources/TFLSearchOptions.h",
    ],
    module_name = "TFLSearchOptions",
    deps = ["//tensorflow_lite_support/ios/task/core:TFLExternalFile"],
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

objc_library(
    name = "TFLEmbeddingOptionsHelpers",
    srcs = [
        "sources/TFLEmbeddingOptions+Helpers.mm",
    ],
    hdrs = [
        "sources/TFLEmbeddingOptions+Helpers.h",
    ],
    module_name = "TFLEmbeddingOptionsHelpers",
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_cc_proto_inc",
        "//tensorflow_lite_support/ios/task/processor:TFLEmbeddingOptions",
    ],
)

objc_library(
    name = "TFLSearchOptionsHelpers",
    srcs = [
        "sources/TFLSearchOptions+Helpers.mm",
    ],
    hdrs = [
        "sources/TFLSearchOptions+Helpers.h",
    ],
    module_name = "TFLSearchOptionsHelpers",
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:search_options_cc_proto_inc",
        "//tensorflow_lite_support/ios/task/processor:TFLSearchOptions",
    ],
)

objc_library(
    name = "TFLSearchResultHelpers",
    srcs = [
        "sources/TFLSearchResult+Helpers.mm",
    ],
    hdrs = [
        "sources/TFLSearchResult+Helpers.h",
    ],
    module_name = "TFLSearchResultHelpers",
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/processor/proto:search_result_cc_proto_inc",
        "//tensorflow_lite_support/ios:TFLCommonCppUtils",
        "//tensorflow_lite_support/ios/task/processor:TFLSearchResult",
    ],
)
