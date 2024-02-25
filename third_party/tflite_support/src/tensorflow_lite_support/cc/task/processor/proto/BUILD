load("//tensorflow_lite_support/cc/port:build_defs.bzl", "support_py_proto_library")

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
)

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

proto_library(
    name = "classifications_proto",
    srcs = ["classifications.proto"],
    deps = [":class_proto"],
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
    py_proto_deps = [
        ":class_py_pb2",
    ],
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

proto_library(
    name = "embedding_proto",
    srcs = ["embedding.proto"],
)

cc_proto_library(
    name = "embedding_cc_proto",
    deps = [
        ":embedding_proto",
    ],
)

support_py_proto_library(
    name = "embedding_py_pb2",
    srcs = ["embedding.proto"],
    api_version = 2,
    proto_deps = [":embedding_proto"],
)

proto_library(
    name = "embedding_options_proto",
    srcs = ["embedding_options.proto"],
)

cc_proto_library(
    name = "embedding_options_cc_proto",
    deps = [
        ":embedding_options_proto",
    ],
)

cc_library(
    name = "embedding_options_cc_proto_inc",
    hdrs = ["embedding_options_proto_inc.h"],
    deps = [
        ":embedding_options_cc_proto",
    ],
)

support_py_proto_library(
    name = "embedding_options_py_pb2",
    srcs = ["embedding_options.proto"],
    api_version = 2,
    proto_deps = [":embedding_options_proto"],
)

proto_library(
    name = "classification_options_proto",
    srcs = ["classification_options.proto"],
)

cc_proto_library(
    name = "classification_options_cc_proto",
    deps = [
        ":classification_options_proto",
    ],
)

support_py_proto_library(
    name = "classification_options_py_pb2",
    srcs = ["classification_options.proto"],
    api_version = 2,
    proto_deps = [":classification_options_proto"],
)

java_lite_proto_library(
    name = "classification_options_java_proto_lite",
    deps = [":classification_options_proto"],
)

java_lite_proto_library(
    name = "embedding_options_java_proto_lite",
    deps = [":embedding_options_proto"],
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

proto_library(
    name = "detection_options_proto",
    srcs = ["detection_options.proto"],
)

cc_proto_library(
    name = "detection_options_cc_proto",
    deps = [":detection_options_proto"],
)

support_py_proto_library(
    name = "detection_options_py_pb2",
    srcs = ["detection_options.proto"],
    api_version = 2,
    proto_deps = [":detection_options_proto"],
)

java_lite_proto_library(
    name = "detection_options_java_proto_lite",
    deps = [":detection_options_proto"],
)

proto_library(
    name = "segmentation_options_proto",
    srcs = ["segmentation_options.proto"],
)

cc_proto_library(
    name = "segmentation_options_cc_proto",
    deps = [":segmentation_options_proto"],
)

support_py_proto_library(
    name = "segmentation_options_py_pb2",
    srcs = ["segmentation_options.proto"],
    api_version = 2,
    proto_deps = [":segmentation_options_proto"],
)

proto_library(
    name = "search_options_proto",
    srcs = ["search_options.proto"],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto",
    ],
)

java_lite_proto_library(
    name = "search_options_java_proto_lite",
    deps = [":search_options_proto"],
)

cc_proto_library(
    name = "search_options_cc_proto",
    deps = [
        ":search_options_proto",
    ],
)

cc_library(
    name = "search_options_cc_proto_inc",
    hdrs = ["search_options_proto_inc.h"],
    deps = [
        ":search_options_cc_proto",
    ],
)

support_py_proto_library(
    name = "search_options_py_pb2",
    srcs = ["search_options.proto"],
    api_version = 2,
    proto_deps = [":search_options_proto"],
    py_proto_deps = [
        "//tensorflow_lite_support/cc/task/core/proto:external_file_py_pb2",
    ],
)

proto_library(
    name = "search_result_proto",
    srcs = ["search_result.proto"],
)

java_lite_proto_library(
    name = "search_result_java_proto_lite",
    deps = [":search_result_proto"],
)

cc_proto_library(
    name = "search_result_cc_proto",
    deps = [":search_result_proto"],
)

cc_library(
    name = "search_result_cc_proto_inc",
    hdrs = ["search_result_proto_inc.h"],
    deps = [
        ":search_result_cc_proto",
    ],
)

support_py_proto_library(
    name = "search_result_py_pb2",
    srcs = ["search_result.proto"],
    api_version = 2,
    proto_deps = [":search_result_proto"],
)

proto_library(
    name = "clu_annotation_options_proto",
    srcs = ["clu_annotation_options.proto"],
)

cc_proto_library(
    name = "clu_annotation_options_cc_proto",
    deps = [":clu_annotation_options_proto"],
)

support_py_proto_library(
    name = "clu_annotation_options_py_pb2",
    srcs = ["clu_annotation_options.proto"],
    api_version = 2,
    proto_deps = [":clu_annotation_options_proto"],
)

proto_library(
    name = "clu_proto",
    srcs = ["clu.proto"],
    deps = [
        ":class_proto",
    ],
)

cc_proto_library(
    name = "clu_cc_proto",
    deps = [":clu_proto"],
)

support_py_proto_library(
    name = "clu_py_pb2",
    srcs = ["clu.proto"],
    api_version = 2,
    proto_deps = [":clu_proto"],
    py_proto_deps = [
        ":class_py_pb2",
    ],
)

proto_library(
    name = "qa_answers_proto",
    srcs = ["qa_answers.proto"],
)

cc_proto_library(
    name = "qa_answers_cc_proto",
    deps = [
        ":qa_answers_proto",
    ],
)

support_py_proto_library(
    name = "qa_answers_py_pb2",
    srcs = ["qa_answers.proto"],
    api_version = 2,
    proto_deps = [":qa_answers_proto"],
)
