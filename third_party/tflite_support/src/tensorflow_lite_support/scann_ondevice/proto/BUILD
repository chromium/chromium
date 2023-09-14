load("//tensorflow_lite_support/cc/port:build_defs.bzl", "support_py_proto_library")

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
)

proto_library(
    name = "index_config_proto",
    srcs = ["index_config.proto"],
    deps = [
        "//tensorflow_lite_support/scann_ondevice/cc/core:serialized_searcher_protolib",
    ],
)

cc_proto_library(
    name = "index_config_cc_proto",
    deps = [
        ":index_config_proto",
    ],
)

support_py_proto_library(
    name = "index_config_py_pb2",
    srcs = ["index_config.proto"],
    api_version = 2,
    proto_deps = [
        ":index_config_proto",
    ],
    py_proto_deps = [
        "//tensorflow_lite_support/scann_ondevice/cc/core:serialized_searcher_py_pb2",
    ],
)
