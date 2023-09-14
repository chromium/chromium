load("//tensorflow_lite_support/cc/port:build_defs.bzl", "support_py_proto_library")

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
)

proto_library(
    name = "external_file_proto",
    srcs = ["external_file.proto"],
)

cc_proto_library(
    name = "external_file_cc_proto",
    deps = [
        ":external_file_proto",
    ],
)

support_py_proto_library(
    name = "external_file_py_pb2",
    srcs = ["external_file.proto"],
    api_version = 2,
    proto_deps = [":external_file_proto"],
)

cc_library(
    name = "external_file_proto_inc",
    hdrs = ["external_file_proto_inc.h"],
    deps = [":external_file_cc_proto"],
)

proto_library(
    name = "base_options_proto",
    srcs = ["base_options.proto"],
    deps = [
        ":external_file_proto",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_proto",
    ],
)

cc_proto_library(
    name = "base_options_cc_proto",
    deps = [
        ":base_options_proto",
    ],
)

support_py_proto_library(
    name = "base_options_py_pb2",
    srcs = ["base_options.proto"],
    api_version = 2,
    proto_deps = [":base_options_proto"],
    py_proto_deps = [
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_proto_external_py",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_py_pb2",
    ],
)

cc_library(
    name = "base_options_proto_inc",
    hdrs = ["base_options_proto_inc.h"],
    deps = [
        ":base_options_cc_proto",
        ":external_file_proto_inc",
        "//tensorflow_lite_support/cc/port:configuration_proto_inc",
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

java_lite_proto_library(
    name = "base_options_java_proto_lite",
    deps = [":base_options_proto"],
)
