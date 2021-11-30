# This package contains implementation of various Function Secret Sharing (FSS)
# gates as specified in https://eprint.iacr.org/2020/1392. The implementation
# uses the Distributed Comparison Function (as implemented in
# distributed_comparison_function.cc) as a central component.

load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

# Multiple Interval Containment

cc_library(
    name = "multiple_interval_containment",
    srcs = ["multiple_interval_containment.cc"],
    hdrs = ["multiple_interval_containment.h"],
    deps = [
        ":multiple_interval_containment_cc_proto",
        "//dcf:distributed_comparison_function",
        "//dcf/fss_gates/prng:basic_rng",
        "//dpf:distributed_point_function_cc_proto",
        "//dpf:status_macros",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

proto_library(
    name = "multiple_interval_containment_proto",
    srcs = ["multiple_interval_containment.proto"],
    deps = [
        "//dcf:distributed_comparison_function_proto",
        "//dpf:distributed_point_function_proto",
    ],
)

cc_proto_library(
    name = "multiple_interval_containment_cc_proto",
    deps = [":multiple_interval_containment_proto"],
)

cc_test(
    name = "multiple_interval_containment_test",
    srcs = ["multiple_interval_containment_test.cc"],
    deps = [
        ":multiple_interval_containment",
        ":multiple_interval_containment_cc_proto",
        "//dcf/fss_gates/prng:basic_rng",
        "//dpf:distributed_point_function_cc_proto",
        "//dpf:status_macros",
        "//dpf/internal:status_matchers",
        "//dpf/internal:value_type_helpers",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/numeric:int128",
    ],
)
