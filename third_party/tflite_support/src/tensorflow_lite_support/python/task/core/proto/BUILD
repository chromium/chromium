load("//tensorflow_lite_support/cc/port:build_defs.bzl", "support_py_proto_library")

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

proto_library(
    name = "base_options_proto",
    srcs = ["base_options.proto"],
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
)
