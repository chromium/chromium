load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite")

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

exports_files(srcs = ["base_vision_task_api.h"])

cc_library_with_tflite(
    name = "base_vision_task_api",
    hdrs = [
        "base_vision_task_api.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/core:base_task_api",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
        "//tensorflow_lite_support/cc/task/vision/utils:image_tensor_specs",
        "//tensorflow_lite_support/cc/task/processor:image_preprocessor",
    ],
    deps = [
        ":frame_buffer",
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_utils",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/time",
        "@org_tensorflow//tensorflow/lite/c:common",
    ],
)

cc_library(
    name = "frame_buffer",
    srcs = ["frame_buffer.cc"],
    hdrs = ["frame_buffer.h"],
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:statusor",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:any",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "label_map_item",
    srcs = ["label_map_item.cc"],
    hdrs = ["label_map_item.h"],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
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
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision/utils:score_calibration",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)
