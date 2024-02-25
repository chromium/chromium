load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite")

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library_with_tflite(
    name = "tflite_engine",
    srcs = ["tflite_engine.cc"],
    hdrs = ["tflite_engine.h"],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/c:common",
        # The dependency on builtin_ops here is only for the default
        # value of the OpResolver parameter:
        #     std::unique_ptr<tflite::IterableOpResolver> resolver =
        #       absl::make_unique<tflite::ops::builtin::BuiltinOpResolver>()
        # When linking statically, if the client of this library doesn't use
        # the default argument, this dependency does not cause all the builtin ops
        # to get included in the executable.
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "@org_tensorflow//tensorflow/lite:framework_stable",
        "@org_tensorflow//tensorflow/lite/tools:verifier",
        "//tensorflow_lite_support/cc/port:tflite_wrapper",
    ],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        ":error_reporter",
        ":external_file_handler",
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:configuration_proto_inc",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite:kernel_api",
        "@org_tensorflow//tensorflow/lite/core/api:op_resolver",
    ],
)

cc_library_with_tflite(
    name = "base_task_api",
    hdrs = ["base_task_api.h"],
    tflite_deps = [
        ":tflite_engine",
        "//tensorflow_lite_support/cc/port:tflite_wrapper",
    ],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite/c:common",
    ],
)

cc_library_with_tflite(
    name = "task_api_factory",
    srcs = ["task_api_factory.cc"],
    hdrs = ["task_api_factory.h"],
    tflite_deps = [
        ":base_task_api",
        ":tflite_engine",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",  # For default value of OpResolver parameter.
    ],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:configuration_proto_inc",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/status",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api:op_resolver",
        "@org_tensorflow//tensorflow/lite/kernels:op_macros",
    ],
)

cc_library(
    name = "task_utils",
    srcs = ["task_utils.cc"],
    hdrs = ["task_utils.h"],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
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
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
)

cc_library(
    name = "external_file_handler",
    srcs = ["external_file_handler.cc"],
    hdrs = ["external_file_handler.h"],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
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

cc_library(
    name = "error_reporter",
    srcs = ["error_reporter.cc"],
    hdrs = ["error_reporter.h"],
    deps = [
        "@org_tensorflow//tensorflow/lite:minimal_logging",
        "@org_tensorflow//tensorflow/lite:stateful_error_reporter",
    ],
)

cc_library(
    name = "label_map_item",
    srcs = ["label_map_item.cc"],
    hdrs = ["label_map_item.h"],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:statusor",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_library(
    name = "classification_head",
    srcs = ["classification_head.cc"],
    hdrs = ["classification_head.h"],
    deps = [
        ":label_map_item",
        ":score_calibration",
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "score_calibration",
    srcs = ["score_calibration.cc"],
    hdrs = ["score_calibration.h"],
    deps = [
        ":label_map_item",
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
    ],
)
