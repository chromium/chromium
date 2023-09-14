package(
    default_visibility = [
        "//tensorflow_lite_support/cc/port:__subpackages__",
        "//tensorflow_lite_support/cc/test:__subpackages__",
    ],
    licenses = ["notice"],  # Apache 2.0
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
    name = "status_matchers",
    testonly = 1,
    hdrs = ["status_matchers.h"],
    deps = [
        "@com_google_googletest//:gtest_main",
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
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/synchronization",
        "@flatbuffers",
        "@org_tensorflow//tensorflow/lite:framework",
        "@org_tensorflow//tensorflow/lite:minimal_logging",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_cc_proto",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:delegate_registry",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:flatbuffer_to_proto",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:nnapi_plugin",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:proto_to_flatbuffer",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/delegates:interpreter_utils",
        "@org_tensorflow//tensorflow/lite/experimental/acceleration/mini_benchmark",
    ] + select({
        # We only intend to use TFLite mini-benchmark on arm-based Andorid and x86_64 Linux.
        "@org_tensorflow//tensorflow:android_arm": [
            "@org_tensorflow//tensorflow/lite/experimental/acceleration/mini_benchmark:mini_benchmark_implementation",
        ],
        "@org_tensorflow//tensorflow:android_arm64": [
            "@org_tensorflow//tensorflow/lite/experimental/acceleration/mini_benchmark:mini_benchmark_implementation",
        ],
        "@org_tensorflow//tensorflow:linux_x86_64": [
            "@org_tensorflow//tensorflow/lite/experimental/acceleration/mini_benchmark:mini_benchmark_implementation",
        ],
        "//conditions:default": [],
    }),
)
