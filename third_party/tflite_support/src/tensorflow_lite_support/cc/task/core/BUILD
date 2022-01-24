package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "tflite_engine",
    srcs = ["tflite_engine.cc"],
    hdrs = ["tflite_engine.h"],
    deps = [
        ":external_file_handler",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api",
        # The dependency on builtin_ops here is only for the default
        # value of the OpResolver parameter:
        #     std::unique_ptr<tflite::IterableOpResolver> resolver =
        #       absl::make_unique<tflite::ops::builtin::BuiltinOpResolver>()
        # When linking statically, if the client of this library doesn't use
        # the default argument, this dependency does not cause all the builtin ops
        # to get included in the executable.
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "@org_tensorflow//tensorflow/lite/tools:verifier",
        "@org_tensorflow//tensorflow/lite:kernel_api",
    ] + select({
        "//tensorflow_lite_support/cc:tflite_use_c_api": [
            "@org_tensorflow//tensorflow/lite/c:c_api",
            "@org_tensorflow//tensorflow/lite/c:c_api_experimental",
            "@org_tensorflow//tensorflow/lite:stderr_reporter",
        ],
        "//conditions:default": ["@org_tensorflow//tensorflow/lite:framework"],
    }) + [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:tflite_wrapper",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
    ],
)

# This is a duplicate of the above 'tflite_engine' target that is used for
# testing with TFLITE_USE_C_API defined.  It should be the same as the target
# above, except that it adds
#    testonly = 1,
#    defines = ["TFLITE_USE_C_API"],
# and that it resolves the conditional deps from the 'select' as if
# "//tensorflow_lite_support/cc:tflite_use_c_api" was enabled.
# This allows testing the TFLITE_USE_C_API case even when
# '--copt=-DTFLITE_USE_C_API' wasn't passed on the build command line.
cc_library(
    name = "tflite_engine_with_c_api_for_test",
    testonly = 1,
    srcs = ["tflite_engine.cc"],
    hdrs = ["tflite_engine.h"],
    defines = ["TFLITE_USE_C_API"],
    deps = [
        ":external_file_handler",
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:tflite_wrapper_with_c_api_for_test",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite:kernel_api",
        "@org_tensorflow//tensorflow/lite:stderr_reporter",
        "@org_tensorflow//tensorflow/lite/c:c_api",
        "@org_tensorflow//tensorflow/lite/c:c_api_experimental",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "@org_tensorflow//tensorflow/lite/tools:verifier",
    ],
)

cc_library(
    name = "base_task_api",
    hdrs = ["base_task_api.h"],
    deps = [
        ":tflite_engine",
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/port:tflite_wrapper",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite/c:common",
    ],
)

cc_library(
    name = "task_api_factory",
    hdrs = ["task_api_factory.h"],
    deps = [
        ":base_task_api",
        ":tflite_engine",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "@com_google_absl//absl/status",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api",
        "@org_tensorflow//tensorflow/lite/kernels:op_macros",
    ],
)

cc_library(
    name = "task_utils",
    srcs = ["task_utils.cc"],
    hdrs = ["task_utils.h"],
    deps = [
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@flatbuffers",
        "@org_tensorflow//tensorflow/lite:string_util",
        "@org_tensorflow//tensorflow/lite:type_to_tflitetype",
        "@org_tensorflow//tensorflow/lite/kernels:op_macros",
        "@org_tensorflow//tensorflow/lite/kernels/internal:tensor",
    ],
)

cc_library(
    name = "category",
    hdrs = ["category.h"],
)

cc_library(
    name = "external_file_handler",
    srcs = ["external_file_handler.cc"],
    hdrs = ["external_file_handler.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)
