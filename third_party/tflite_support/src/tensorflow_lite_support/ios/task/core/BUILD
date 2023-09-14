package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

objc_library(
    name = "TFLExternalFile",
    srcs = [
        "sources/TFLExternalFile.m",
    ],
    hdrs = [
        "sources/TFLExternalFile.h",
    ],
    module_name = "TFLExternalFile",
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
    deps = [
        ":TFLExternalFile",
    ],
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

objc_library(
    name = "TFLBaseOptionsCppHelpers",
    srcs = [
        "sources/TFLBaseOptions+CppHelpers.mm",
    ],
    hdrs = [
        "sources/TFLBaseOptions+CppHelpers.h",
    ],
    module_name = "TFLBaseOptionsCppHelpers",
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/ios/task/core:TFLBaseOptions",
    ],
)
