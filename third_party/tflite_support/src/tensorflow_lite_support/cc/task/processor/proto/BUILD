load("//tensorflow_lite_support/cc/port:build_defs.bzl", "support_py_proto_library")

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
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
