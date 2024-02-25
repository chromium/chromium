load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite")

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "statusor",
    hdrs = [
        "statusor.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = [
        "status_macros.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/cc/port/default:status_macros",
    ],
)

cc_library(
    name = "configuration_proto_inc",
    hdrs = ["configuration_proto_inc.h"],
    deps = ["@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_cc_proto"],
)

cc_library_with_tflite(
    name = "tflite_wrapper",
    hdrs = ["tflite_wrapper.h"],
    deps = ["//tensorflow_lite_support/cc/port/default:tflite_wrapper"],
)

cc_library(
    name = "integral_types",
    hdrs = ["integral_types.h"],
    visibility = ["//tensorflow_lite_support:users"],
)

cc_library(
    name = "gtest_main",
    testonly = 1,
    hdrs = [
        "benchmark.h",
        "gmock.h",
        "gtest.h",
        "status_matchers.h",
    ],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port/default:status_matchers",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "proto2",
    hdrs = [
        "proto2.h",
    ],
    deps = [
        "@com_google_protobuf//:protobuf",
    ],
)
