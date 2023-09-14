package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_binary(
    name = "image_classifier_demo",
    srcs = ["image_classifier_demo.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:external_file_handler",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "//tensorflow_lite_support/cc/task/vision:image_classifier",
        "//tensorflow_lite_support/cc/task/vision/proto:class_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:classifications_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_classifier_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ] + select({
        "//tensorflow_lite_support/examples/task:darwinn_portable": [
            "//tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin",
        ],
        "//conditions:default": [
        ],
    }),
)

cc_binary(
    name = "object_detector_demo",
    srcs = ["object_detector_demo.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:external_file_handler",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "//tensorflow_lite_support/cc/task/vision:object_detector",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:class_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:detections_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:object_detector_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ] + select({
        "//tensorflow_lite_support/examples/task:darwinn_portable": [
            "//tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin",
        ],
        "//conditions:default": [
        ],
    }),
)

cc_binary(
    name = "image_segmenter_demo",
    srcs = ["image_segmenter_demo.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:external_file_handler",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "//tensorflow_lite_support/cc/task/vision:image_segmenter",
        "//tensorflow_lite_support/cc/task/vision/proto:image_segmenter_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:segmentations_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ] + select({
        "//tensorflow_lite_support/examples/task:darwinn_portable": [
            "//tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin",
        ],
        "//conditions:default": [
        ],
    }),
)

cc_binary(
    name = "image_embedder_demo",
    srcs = ["image_embedder_demo.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:external_file_handler",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "//tensorflow_lite_support/cc/task/vision:image_embedder",
        "//tensorflow_lite_support/cc/task/vision/proto:embeddings_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_embedder_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ] + select({
        "//tensorflow_lite_support/examples/task:darwinn_portable": [
            "//tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin",
        ],
        "//conditions:default": [
        ],
    }),
)

cc_binary(
    name = "image_searcher_demo",
    srcs = ["image_searcher_demo.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:external_file_handler",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_cc_proto",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_result_cc_proto",
        "//tensorflow_lite_support/cc/task/vision:image_searcher",
        "//tensorflow_lite_support/cc/task/vision/proto:image_searcher_options_cc_proto",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ] + select({
        "//tensorflow_lite_support/examples/task:darwinn_portable": [
            "//tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin",
        ],
        "//conditions:default": [
        ],
    }),
)
