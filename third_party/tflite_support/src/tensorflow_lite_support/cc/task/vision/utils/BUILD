load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite")

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "score_calibration",
    srcs = ["score_calibration.cc"],
    hdrs = ["score_calibration.h"],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision/core:label_map_item",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
    ],
)

cc_library(
    name = "frame_buffer_common_utils",
    srcs = [
        "frame_buffer_common_utils.cc",
    ],
    hdrs = [
        "frame_buffer_common_utils.h",
        "frame_buffer_utils_interface.h",
    ],
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/time",
    ],
)

cc_library(
    name = "frame_buffer_utils",
    srcs = [
        "frame_buffer_utils.cc",
    ],
    hdrs = [
        "frame_buffer_utils.h",
    ],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        ":frame_buffer_common_utils",
        ":libyuv_frame_buffer_utils",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:variant",
        "@com_google_glog//:glog",
        "@org_tensorflow//tensorflow/lite/kernels:op_macros",
        "@org_tensorflow//tensorflow/lite/kernels/internal:compatibility",
    ],
)

cc_library(
    name = "libyuv_frame_buffer_utils",
    srcs = ["libyuv_frame_buffer_utils.cc"],
    hdrs = ["libyuv_frame_buffer_utils.h"],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        ":frame_buffer_common_utils",
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@libyuv",
    ],
)

cc_library_with_tflite(
    name = "image_tensor_specs",
    srcs = ["image_tensor_specs.cc"],
    hdrs = ["image_tensor_specs.h"],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
        "@com_google_absl//absl/types:optional",
        "@org_tensorflow//tensorflow/lite/c:common",
    ],
)

cc_library(
    name = "image_utils",
    srcs = ["image_utils.cc"],
    hdrs = ["image_utils.h"],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        ":frame_buffer_common_utils",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@stblib//:stb_image",
        "@stblib//:stb_image_write",
    ],
)
