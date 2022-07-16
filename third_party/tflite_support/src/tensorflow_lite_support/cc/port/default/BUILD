package(
    default_visibility = [
        "//tensorflow_lite_support/cc/port:__pkg__",
        "//tensorflow_lite_support/cc/test:__pkg__",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "statusor",
    srcs = ["statusor.cc"],
    hdrs = [
        "statusor.h",
        "statusor_internals.h",
    ],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:variant",
        "@com_google_absl//absl/utility",
        "@com_google_glog//:glog",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = [
        "status_macros.h",
    ],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "tflite_wrapper",
    srcs = ["tflite_wrapper.cc"],
    hdrs = [
        "tflite_wrapper.h",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:status_macros",
        "@com_google_absl//absl/status",
        "@org_tensorflow//tensorflow/lite:framework",
        "@org_tensorflow//tensorflow/lite/experimental/acceleration/configuration:configuration_cc_proto",
    ],
)
