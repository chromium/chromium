load("//tensorflow_lite_support/cc/port:build_defs.bzl", "support_py_proto_library")

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
)

# Common vision protos.

proto_library(
    name = "bounding_box_proto",
    srcs = ["bounding_box.proto"],
)

cc_proto_library(
    name = "bounding_box_cc_proto",
    deps = [
        ":bounding_box_proto",
    ],
)

support_py_proto_library(
    name = "bounding_box_py_pb2",
    srcs = ["bounding_box.proto"],
    api_version = 2,
    proto_deps = [":bounding_box_proto"],
)

cc_library(
    name = "bounding_box_proto_inc",
    hdrs = ["bounding_box_proto_inc.h"],
    deps = [":bounding_box_cc_proto"],
)

proto_library(
    name = "class_proto",
    srcs = ["class.proto"],
)

cc_proto_library(
    name = "class_cc_proto",
    deps = [
        ":class_proto",
    ],
)

support_py_proto_library(
    name = "class_py_pb2",
    srcs = ["class.proto"],
    api_version = 2,
    proto_deps = [":class_proto"],
)

cc_library(
    name = "class_proto_inc",
    hdrs = ["class_proto_inc.h"],
    deps = [":class_cc_proto"],
)

# ObjectDetector protos.

proto_library(
    name = "object_detector_options_proto",
    srcs = ["object_detector_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_proto",
    ],
)

cc_proto_library(
    name = "object_detector_options_cc_proto",
    deps = [
        ":object_detector_options_proto",
    ],
)

support_py_proto_library(
    name = "object_detector_options_py_pb2",
    srcs = ["object_detector_options.proto"],
    api_version = 2,
    proto_deps = [":object_detector_options_proto"],
    py_proto_deps = [
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_proto_external_py",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_py_pb2",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_py_pb2",
    ],
)

cc_library(
    name = "object_detector_options_proto_inc",
    hdrs = ["object_detector_options_proto_inc.h"],
    deps = [
        ":object_detector_options_cc_proto",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
    ],
)

proto_library(
    name = "detections_proto",
    srcs = ["detections.proto"],
    deps = [
        ":bounding_box_proto",
        ":class_proto",
    ],
)

cc_proto_library(
    name = "detections_cc_proto",
    deps = [
        ":detections_proto",
    ],
)

support_py_proto_library(
    name = "detections_py_pb2",
    srcs = ["detections.proto"],
    api_version = 2,
    proto_deps = [":detections_proto"],
    py_proto_deps = [
        ":class_py_pb2",
        ":bounding_box_py_pb2",
    ],
)

cc_library(
    name = "detections_proto_inc",
    hdrs = ["detections_proto_inc.h"],
    deps = [
        ":bounding_box_proto_inc",
        ":class_proto_inc",
        ":detections_cc_proto",
    ],
)

# ImageClassifier protos.

proto_library(
    name = "image_classifier_options_proto",
    srcs = ["image_classifier_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_proto",
    ],
)

cc_proto_library(
    name = "image_classifier_options_cc_proto",
    deps = [
        ":image_classifier_options_proto",
    ],
)

support_py_proto_library(
    name = "image_classifier_options_py_pb2",
    srcs = ["image_classifier_options.proto"],
    api_version = 2,
    proto_deps = [":image_classifier_options_proto"],
    py_proto_deps = [
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_proto_external_py",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_py_pb2",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_py_pb2",
    ],
)

cc_library(
    name = "image_classifier_options_proto_inc",
    hdrs = ["image_classifier_options_proto_inc.h"],
    deps = [
        ":image_classifier_options_cc_proto",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
    ],
)

proto_library(
    name = "classifications_proto",
    srcs = ["classifications.proto"],
    deps = [
        ":class_proto",
    ],
)

cc_proto_library(
    name = "classifications_cc_proto",
    deps = [
        ":classifications_proto",
    ],
)

support_py_proto_library(
    name = "classifications_py_pb2",
    srcs = ["classifications.proto"],
    api_version = 2,
    proto_deps = [":classifications_proto"],
    py_proto_deps = [":class_py_pb2"],
)

cc_library(
    name = "classifications_proto_inc",
    hdrs = ["classifications_proto_inc.h"],
    deps = [
        ":class_proto_inc",
        ":classifications_cc_proto",
    ],
)

# ImageSegmenter protos.

proto_library(
    name = "image_segmenter_options_proto",
    srcs = ["image_segmenter_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_proto",
    ],
)

cc_proto_library(
    name = "image_segmenter_options_cc_proto",
    deps = [
        ":image_segmenter_options_proto",
    ],
)

cc_library(
    name = "image_segmenter_options_proto_inc",
    hdrs = ["image_segmenter_options_proto_inc.h"],
    deps = [
        ":image_segmenter_options_cc_proto",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
    ],
)

proto_library(
    name = "segmentations_proto",
    srcs = ["segmentations.proto"],
)

cc_proto_library(
    name = "segmentations_cc_proto",
    deps = [
        ":segmentations_proto",
    ],
)

support_py_proto_library(
    name = "segmentations_py_pb2",
    srcs = ["segmentations.proto"],
    api_version = 2,
    proto_deps = [":segmentations_proto"],
)

cc_library(
    name = "segmentations_proto_inc",
    hdrs = ["segmentations_proto_inc.h"],
    deps = [":segmentations_cc_proto"],
)

# ImageEmbedder protos.

proto_library(
    name = "image_embedder_options_proto",
    srcs = ["image_embedder_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_proto",
    ],
)

cc_proto_library(
    name = "image_embedder_options_cc_proto",
    deps = [
        ":image_embedder_options_proto",
    ],
)

support_py_proto_library(
    name = "image_embedder_options_py_pb2",
    srcs = ["image_embedder_options.proto"],
    api_version = 2,
    proto_deps = [":image_embedder_options_proto"],
    py_proto_deps = [
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_proto_external_py",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_py_pb2",
    ],
)

cc_library(
    name = "image_embedder_options_proto_inc",
    hdrs = ["image_embedder_options_proto_inc.h"],
    deps = [
        ":image_embedder_options_cc_proto",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
    ],
)

proto_library(
    name = "embeddings_proto",
    srcs = ["embeddings.proto"],
)

cc_proto_library(
    name = "embeddings_cc_proto",
    deps = [
        ":embeddings_proto",
    ],
)

support_py_proto_library(
    name = "embeddings_py_pb2",
    srcs = ["embeddings.proto"],
    api_version = 2,
    proto_deps = [":embeddings_proto"],
)

cc_library(
    name = "embeddings_proto_inc",
    hdrs = ["embeddings_proto_inc.h"],
    deps = [":embeddings_cc_proto"],
)

proto_library(
    name = "image_searcher_options_proto",
    srcs = ["image_searcher_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:embedding_options_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_options_proto",
    ],
)

cc_proto_library(
    name = "image_searcher_options_cc_proto",
    deps = [
        ":image_searcher_options_proto",
    ],
)
