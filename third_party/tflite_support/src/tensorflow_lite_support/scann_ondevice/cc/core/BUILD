load("//tensorflow_lite_support/cc/port:build_defs.bzl", "support_py_proto_library")

package(
    default_visibility = [
        "//tensorflow_lite_support:__subpackages__",
    ],
    licenses = ["notice"],
)

proto_library(
    name = "serialized_searcher_protolib",
    srcs = ["serialized_searcher.proto"],
    deps = [],
)

cc_proto_library(
    name = "serialized_searcher_cc_proto",
    deps = [":serialized_searcher_protolib"],
)

java_proto_library(
    name = "serialized_searcher_java_proto",
    deps = [":serialized_searcher_protolib"],
)

java_lite_proto_library(
    name = "serialized_searcher_java_proto_lite",
    deps = [":serialized_searcher_protolib"],
)

support_py_proto_library(
    name = "serialized_searcher_py_pb2",
    srcs = ["serialized_searcher.proto"],
    api_version = 2,
    proto_deps = [":serialized_searcher_protolib"],
)

cc_library(
    name = "top_n_amortized_constant",
    hdrs = ["top_n_amortized_constant.h"],
    deps = [
        "@com_google_glog//:glog",
    ],
)

cc_library(
    name = "searcher",
    srcs = ["searcher.cc"],
    hdrs = [
        "index_table_sum.h",
        "searcher.h",
        "simd_utils.h",
    ],
    deps = [
        ":processor",
        ":serialized_searcher_cc_proto",
        ":top_n_amortized_constant",
        "//tensorflow_lite_support/cc/port:integral_types",
        "@com_google_absl//absl/types:span",
        "@com_google_glog//:glog",
        "@eigen//:eigen3",
    ],
)

cc_library(
    name = "partitioner",
    srcs = ["partitioner.cc"],
    hdrs = ["partitioner.h"],
    deps = [
        ":serialized_searcher_cc_proto",
        "@com_google_absl//absl/types:optional",
        "@com_google_glog//:glog",
        "@eigen//:eigen3",
    ],
)

cc_library(
    name = "processor",
    srcs = ["processor.cc"],
    hdrs = [
        "index_table_sum.h",
        "processor.h",
        "simd_utils.h",
    ],
    deps = [
        ":serialized_searcher_cc_proto",
        ":top_n_amortized_constant",
        "//tensorflow_lite_support/cc/port:integral_types",
        "@com_google_glog//:glog",
        "@eigen//:eigen3",
    ],
)

cc_test(
    name = "searcher_test",
    srcs = ["searcher_test.cc"],
    tags = ["not_run:arm"],
    deps = [
        ":partitioner",
        ":processor",
        ":searcher",
        ":serialized_searcher_cc_proto",
        ":top_n_amortized_constant",
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:proto2",
        "@com_google_absl//absl/synchronization",
        "@com_google_glog//:glog",
        "@eigen//:eigen3",
    ],
)

cc_library(
    name = "indexer",
    srcs = ["indexer.cc"],
    hdrs = ["indexer.h"],
    deps = [
        ":serialized_searcher_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
        "@com_google_glog//:glog",
    ],
)

cc_test(
    name = "indexer_test",
    srcs = ["indexer_test.cc"],
    deps = [
        ":indexer",
        ":serialized_searcher_cc_proto",
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/port:proto2",
        "@com_google_glog//:glog",
    ],
)
