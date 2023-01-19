load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")
load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

cc_library(
    name = "int_mod_n",
    srcs = ["int_mod_n.cc"],
    hdrs = ["int_mod_n.h"],
    deps = [
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
    ],
)

cc_test(
    name = "int_mod_n_test",
    srcs = ["int_mod_n_test.cc"],
    deps = [
        ":int_mod_n",
        "//dpf/internal:status_matchers",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "int_mod_n_benchmark",
    srcs = ["int_mod_n_benchmark.cc"],
    deps = [
        ":int_mod_n",
        "@boringssl//:crypto",
        "@com_github_google_benchmark//:benchmark_main",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "distributed_point_function",
    srcs = ["distributed_point_function.cc"],
    hdrs = ["distributed_point_function.h"],
    deps = [
        ":aes_128_fixed_key_hash",
        ":distributed_point_function_cc_proto",
        ":status_macros",
        "//dpf/internal:evaluate_prg_hwy",
        "//dpf/internal:get_hwy_mode",
        "//dpf/internal:proto_validator",
        "//dpf/internal:value_type_helpers",
        "@boringssl//:crypto",
        "@com_github_google_glog//:glog",
        "@com_github_google_highway//:hwy",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "distributed_point_function_test",
    size = "medium",
    srcs = ["distributed_point_function_test.cc"],
    deps = [
        ":distributed_point_function",
        ":distributed_point_function_cc_proto",
        "//dpf/internal:status_matchers",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/random",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/utility",
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
        "@com_github_google_glog//:glog",
        "@com_github_google_highway//:hwy",
        "@com_google_absl//absl/container:btree",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/random",
    ],
)

cc_library(
    name = "status_macros",
    hdrs = ["status_macros.h"],
)

cc_library(
    name = "aes_128_fixed_key_hash",
    srcs = ["aes_128_fixed_key_hash.cc"],
    hdrs = ["aes_128_fixed_key_hash.h"],
    deps = [
        "@boringssl//:crypto",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "aes_128_fixed_key_hash_test",
    srcs = ["aes_128_fixed_key_hash_test.cc"],
    deps = [
        ":aes_128_fixed_key_hash",
        "//dpf/internal:status_matchers",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "tuple",
    hdrs = ["tuple.h"],
    deps = [
        ":distributed_point_function_cc_proto",
    ],
)

cc_test(
    name = "tuple_test",
    srcs = [
        "tuple_test.cc",
    ],
    deps = [
        ":tuple",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/numeric:int128",
    ],
)

cc_library(
    name = "xor_wrapper",
    hdrs = ["xor_wrapper.h"],
)

cc_test(
    name = "xor_wrapper_test",
    srcs = [
        "xor_wrapper_test.cc",
    ],
    deps = [
        ":xor_wrapper",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/numeric:int128",
    ],
)
