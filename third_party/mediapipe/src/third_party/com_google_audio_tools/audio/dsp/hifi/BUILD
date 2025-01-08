package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "dynamic_range_control_functions",
    hdrs = ["dynamic_range_control_functions.h"],
)

cc_library(
    name = "dynamic_range_control",
    srcs = ["dynamic_range_control.cc"],
    hdrs = ["dynamic_range_control.h"],
    deps = [
        ":dynamic_range_control_functions",
        "//audio/dsp:attack_release_envelope",
        "//audio/dsp:decibels",
        "//audio/dsp:fixed_delay_line",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "dynamic_range_control_test",
    size = "small",
    srcs = ["dynamic_range_control_test.cc"],
    deps = [
        ":dynamic_range_control",
        ":dynamic_range_control_functions",
        "//audio/dsp:decibels",
        "//audio/dsp:testing_util",
        "//third_party/eigen3",
        "@com_google_benchmark//:benchmark",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "multi_crossover_filter",
    hdrs = ["multi_crossover_filter.h"],
    deps = [
        "//audio/linear_filters:crossover",
        "//audio/linear_filters:ladder_filter",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
    ],
)

cc_library(
    name = "multiband_compressor",
    srcs = ["multiband_compressor.cc"],
    hdrs = ["multiband_compressor.h"],
    deps = [
        ":dynamic_range_control",
        ":multi_crossover_filter",
        "//audio/linear_filters:crossover",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
    ],
)
