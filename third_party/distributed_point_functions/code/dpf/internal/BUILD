load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")
load("@com_github_google_iree//build_tools/embed_data:build_defs.bzl", "cc_embed_data")

package(
    default_visibility = ["//:__subpackages__"],
)

licenses(["notice"])

cc_library(
    name = "value_type_helpers",
    srcs = ["value_type_helpers.cc"],
    hdrs = ["value_type_helpers.h"],
    deps = [
        "//dpf:distributed_point_function_cc_proto",
        "//dpf:int_mod_n",
        "//dpf:status_macros",
        "//dpf:tuple",
        "//dpf:xor_wrapper",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/utility",
    ],
)

cc_test(
    name = "value_type_helpers_test",
    srcs = ["value_type_helpers_test.cc"],
    deps = [
        ":status_matchers",
        ":value_type_helpers",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "status_matchers",
    testonly = 1,
    srcs = [
        "status_matchers.cc",
    ],
    hdrs = ["status_matchers.h"],
    deps = [
        "//dpf:status_macros",
        "@com_github_google_googletest//:gtest",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "proto_validator",
    srcs = [
        "proto_validator.cc",
    ],
    hdrs = [
        "proto_validator.h",
    ],
    deps = [
        ":value_type_helpers",
        "//dpf:distributed_point_function_cc_proto",
        "//dpf:status_macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_embed_data(
    name = "proto_validator_test_textproto_embed",
    srcs = [
        "proto_validator_test.textproto",
    ],
    cc_file_output = "proto_validator_test_textproto_embed.cc",
    cpp_namespace = "distributed_point_functions::dpf_internal",
    h_file_output = "proto_validator_test_textproto_embed.h",
)

cc_test(
    name = "proto_validator_test",
    srcs = [
        "proto_validator_test.cc",
    ],
    data = [
        "proto_validator_test.textproto",
    ],
    deps = [
        ":proto_validator",
        ":proto_validator_test_textproto_embed",
        ":status_matchers",
        "//dpf:tuple",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_protobuf//:protobuf",
    ],
)
