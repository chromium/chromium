load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")

package(
    default_visibility = ["//dpf:__subpackages__"],
)

licenses(["notice"])

cc_library(
    name = "array_conversions",
    hdrs = ["array_conversions.h"],
    deps = [
        "@com_google_absl//absl/base",
        "@com_google_absl//absl/numeric:int128",
    ],
)

cc_test(
    name = "array_conversions_test",
    srcs = ["array_conversions_test.cc"],
    deps = [
        ":array_conversions",
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
    name = "pseudorandom_generator",
    srcs = ["pseudorandom_generator.cc"],
    hdrs = ["pseudorandom_generator.h"],
    deps = [
        "@boringssl//:crypto",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_test(
    name = "pseudorandom_generator_test",
    srcs = ["pseudorandom_generator_test.cc"],
    deps = [
        ":pseudorandom_generator",
        ":status_matchers",
        "@com_github_google_googletest//:gtest_main",
    ],
)
