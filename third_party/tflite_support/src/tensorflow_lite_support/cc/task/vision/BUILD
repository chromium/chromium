load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite")

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
)

# IMPORTANT: in order to use hardware acceleration delegates, configurable through the
# `compute_settings` field of the ObjectDetectorOptions, you must additionally link to
# the appropriate delegate plugin target (e.g. `gpu_plugin` for GPU) from:
# https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/acceleration/configuration/BUILD
# To use EDGETPU_CORAL, link to `edgetpu_coral_plugin` from:
# https://github.com/tensorflow/tflite-support/blob/a58a4f9225c411fa9ba29f821523e6e283988d23/tensorflow_lite_support/acceleration/configuration/BUILD#L11
cc_library_with_tflite(
    name = "object_detector",
    srcs = ["object_detector.cc"],
    hdrs = ["object_detector.h"],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
        "//tensorflow_lite_support/cc/task/vision/core:base_vision_task_api",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:external_file_handler",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/core:label_map_item",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:class_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:detections_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:object_detector_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_utils",
        "//tensorflow_lite_support/cc/task/vision/utils:score_calibration",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_glog//:glog",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api",
    ],
)

# IMPORTANT: in order to use hardware acceleration delegates, configurable through the
# `compute_settings` field of the ImageClassifierOptions, you must additionally link to
# the appropriate delegate plugin target (e.g. `gpu_plugin` for GPU) from:
# https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/acceleration/configuration/BUILD
# To use EDGETPU_CORAL, link to `edgetpu_coral_plugin` from:
# https://github.com/tensorflow/tflite-support/blob/a58a4f9225c411fa9ba29f821523e6e283988d23/tensorflow_lite_support/acceleration/configuration/BUILD#L11
cc_library_with_tflite(
    name = "image_classifier",
    srcs = ["image_classifier.cc"],
    hdrs = ["image_classifier.h"],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
        "//tensorflow_lite_support/cc/task/vision/core:base_vision_task_api",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:external_file_handler",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/vision/core:classification_head",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/core:label_map_item",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:class_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:classifications_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_classifier_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_utils",
        "//tensorflow_lite_support/cc/task/vision/utils:score_calibration",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/container:flat_hash_set",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@flatbuffers",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api",
    ],
)

# IMPORTANT: in order to use hardware acceleration delegates, configurable through the
# `compute_settings` field of the ImageSegmenterOptions, you must additionally link to
# the appropriate delegate plugin target (e.g. `gpu_plugin` for GPU) from:
# https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/acceleration/configuration/BUILD
# To use EDGETPU_CORAL, link to `edgetpu_coral_plugin` from:
# https://github.com/tensorflow/tflite-support/blob/a58a4f9225c411fa9ba29f821523e6e283988d23/tensorflow_lite_support/acceleration/configuration/BUILD#L11
cc_library_with_tflite(
    name = "image_segmenter",
    srcs = ["image_segmenter.cc"],
    hdrs = ["image_segmenter.h"],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/vision/core:base_vision_task_api",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:external_file_handler",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/core:label_map_item",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_segmenter_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:segmentations_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_utils",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@flatbuffers",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api",
    ],
)

# IMPORTANT: in order to use hardware acceleration delegates, configurable through the
# `compute_settings` field of the ImageEmbedderOptions, you must additionally link to
# the appropriate delegate plugin target (e.g. `gpu_plugin` for GPU) from:
# https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/acceleration/configuration/BUILD
# To use EDGETPU_CORAL, link to `edgetpu_coral_plugin` from:
# https://github.com/tensorflow/tflite-support/blob/a58a4f9225c411fa9ba29f821523e6e283988d23/tensorflow_lite_support/acceleration/configuration/BUILD#L11
cc_library_with_tflite(
    name = "image_embedder",
    srcs = ["image_embedder.cc"],
    hdrs = ["image_embedder.h"],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
        "//tensorflow_lite_support/cc/task/vision/core:base_vision_task_api",
        "//tensorflow_lite_support/cc/task/processor:embedding_postprocessor",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:external_file_handler",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:embeddings_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_embedder_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_utils",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api:op_resolver",
    ],
)

# IMPORTANT: in order to use hardware acceleration delegates, configurable through the
# `compute_settings` field of the ImageSearcherOptions, you must additionally link to
# the appropriate delegate plugin target (e.g. `gpu_plugin` for GPU) from:
# https://github.com/tensorflow/tensorflow/blob/master/tensorflow/lite/acceleration/configuration/BUILD
# To use EDGETPU_CORAL, link to `edgetpu_coral_plugin` from:
# https://github.com/tensorflow/tflite-support/blob/a58a4f9225c411fa9ba29f821523e6e283988d23/tensorflow_lite_support/acceleration/configuration/BUILD#L11
cc_library_with_tflite(
    name = "image_searcher",
    srcs = ["image_searcher.cc"],
    hdrs = ["image_searcher.h"],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
        "//tensorflow_lite_support/cc/task/processor:search_postprocessor",
        "//tensorflow_lite_support/cc/task/vision/core:base_vision_task_api",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_result_cc_proto",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_searcher_options_cc_proto",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_utils",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api:op_resolver",
    ],
)
