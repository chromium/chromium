package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "statusor",
    hdrs = [
        "statusor.h",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port/default:statusor",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = [
        "status_macros.h",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port/default:status_macros",
    ],
)

cc_library(
    name = "tflite_wrapper",
    hdrs = ["tflite_wrapper.h"],
    deps = ["//tensorflow_lite_support/cc/port/default:tflite_wrapper"],
)

# This is identical to the rule above, except that it gets built with
# '-DTFLITE_USE_C_API'. This rule is used for unit tests that verify things
# work correctly when built with TFLITE_USE_C_API defined.
cc_library(
    name = "tflite_wrapper_with_c_api_for_test",
    testonly = 1,
    hdrs = ["tflite_wrapper.h"],
    deps = [
        "//intelligence/mobile_acceleration/proto:allowlist_portable_proto",
        "//intelligence/mobile_acceleration/support_library:tflite_wrapper_with_c_api_for_test",
    ],
)

cc_library(
    name = "integral_types",
    hdrs = ["integral_types.h"],
)
