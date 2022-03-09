package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "category",
    srcs = ["category.cc"],
    hdrs = ["category.h"],
)

cc_library(
    name = "classification_result",
    srcs = [
        "classification_result.cc",
    ],
    hdrs = ["classification_result.h"],
    deps = [
        ":category",
    ],
)

cc_library(
    name = "bounding_box",
    hdrs = ["bounding_box.h"],
)

cc_library(
    name = "classification_options",
    hdrs = ["classification_options.h"],
)

cc_library(
    name = "detection_result",
    srcs = [
        "detection_result.cc",
    ],
    hdrs = ["detection_result.h"],
    deps = [
        ":bounding_box",
        ":category",
    ],
)

cc_library(
    name = "segmentation_result",
    srcs = [
        "segmentation_result.cc",
    ],
    hdrs = ["segmentation_result.h"],
)
