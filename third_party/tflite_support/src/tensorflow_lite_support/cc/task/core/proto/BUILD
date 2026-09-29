load("//third_party/bazel_rules/rules_cc/cc:cc_library.bzl", "cc_library")

# Placeholder: load java_lite_proto_library
# Placeholder: load proto_library
load("//third_party/protobuf/build_defs:kt_jvm_proto_library.bzl", "kt_jvm_lite_proto_library")

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

kt_jvm_lite_proto_library(
    name = "base_options_kt_proto_lite",
    deps = [":base_options_proto"],
)
