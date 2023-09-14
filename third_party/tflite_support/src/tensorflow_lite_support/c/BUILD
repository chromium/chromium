package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "common",
    srcs = ["common.cc"],
    hdrs = ["common.h"],
)

cc_library(
    name = "common_utils",
    srcs = ["common_utils.cc"],
    hdrs = ["common_utils.h"],
    deps = [
        ":common",
        "//tensorflow_lite_support/cc:common",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)
