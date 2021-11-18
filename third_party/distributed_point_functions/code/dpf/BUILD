load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

cc_library(
    name = "distributed_point_function",
    srcs = ["distributed_point_function.cc"],
    hdrs = ["distributed_point_function.h"],
    deps = [
        ":distributed_point_function_cc_proto",
        ":status_macros",
        "//dpf/internal:array_conversions",
        "//dpf/internal:pseudorandom_generator",
        "@boringssl//:crypto",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "distributed_point_function_test",
    size = "medium",
    srcs = ["distributed_point_function_test.cc"],
    deps = [
        ":distributed_point_function",
        "//dpf/internal:status_matchers",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/random",
    ],
)

proto_library(
    name = "distributed_point_function_proto",
    srcs = ["distributed_point_function.proto"],
)

cc_proto_library(
    name = "distributed_point_function_cc_proto",
    deps = [":distributed_point_function_proto"],
)

go_proto_library(
    name = "distributed_point_function_go_proto",
    importpath = "github.com/google/distributed_point_functions/dpf/distributed_point_function_go_proto",
    protos = [":distributed_point_function_proto"],
)

cc_test(
    name = "distributed_point_function_benchmark",
    srcs = [
        "distributed_point_function_benchmark.cc",
    ],
    tags = ["benchmark"],
    deps = [
        ":distributed_point_function",
        "@com_github_google_benchmark//:benchmark_main",
        "@com_google_absl//absl/random",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = ["status_macros.h"],
)
