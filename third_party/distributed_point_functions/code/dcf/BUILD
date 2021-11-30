load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

cc_library(
    name = "distributed_comparison_function",
    srcs = ["distributed_comparison_function.cc"],
    hdrs = ["distributed_comparison_function.h"],
    deps = [
        ":distributed_comparison_function_cc_proto",
        "//dpf:distributed_point_function",
        "//dpf:distributed_point_function_cc_proto",
        "//dpf:status_macros",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status:statusor",
    ],
)

proto_library(
    name = "distributed_comparison_function_proto",
    srcs = ["distributed_comparison_function.proto"],
    deps = [
        "//dpf:distributed_point_function_proto",
    ],
)

cc_proto_library(
    name = "distributed_comparison_function_cc_proto",
    deps = [":distributed_comparison_function_proto"],
)

cc_test(
    name = "distributed_comparison_function_test",
    srcs = ["distributed_comparison_function_test.cc"],
    deps = [
        ":distributed_comparison_function",
        "//dpf/internal:status_matchers",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/utility",
    ],
)
