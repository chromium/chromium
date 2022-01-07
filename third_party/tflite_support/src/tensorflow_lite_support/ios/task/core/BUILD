package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLBaseOptions",
    srcs = [
        "sources/TFLBaseOptions.m",
    ],
    hdrs = [
        "sources/TFLBaseOptions.h",
    ],
    module_name = "TFLBaseOptions",
)

objc_library(
    name = "TFLBaseOptionsHelpers",
    srcs = [
        "sources/TFLBaseOptions+Helpers.m",
    ],
    hdrs = [
        "sources/TFLBaseOptions+Helpers.h",
    ],
    module_name = "TFLBaseOptionsHelpers",
    deps = [
        "//tensorflow_lite_support/c/task/core:base_options",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptions",
    ],
)
