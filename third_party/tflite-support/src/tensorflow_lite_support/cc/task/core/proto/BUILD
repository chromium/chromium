load("//tensorflow_lite_support/cc/port:build_defs.bzl", "support_cc_proto_library")

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

support_cc_proto_library(
    name = "external_file_cc_proto",
    srcs = ["external_file.proto"],
    deps = [
        ":external_file_proto",
    ],
)

cc_library(
    name = "external_file_proto_inc",
    hdrs = ["external_file_proto_inc.h"],
    deps = [":external_file_cc_proto"],
)
