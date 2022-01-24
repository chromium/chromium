package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "common",
    srcs = [
        "common.cc",
    ],
    hdrs = ["common.h"],
    deps = [
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
    ],
)

config_setting(
    name = "tflite_use_c_api",
    values = {
        "copt": "-DTFLITE_USE_C_API",
    },
    visibility = ["//tensorflow_lite_support:__subpackages__"],
)
