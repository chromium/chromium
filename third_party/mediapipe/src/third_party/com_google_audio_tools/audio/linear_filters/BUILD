# Description:
#   Utilities for generic linear filtering.
#
# TODO: OnePoleFilter class to complement BiquadFilter, so
# that we can decompose odd-order filters into biquad stages + one-pole stage.

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "biquad_filter",
    srcs = ["biquad_filter-inl.h"],
    hdrs = ["biquad_filter.h"],
    deps = [
        ":biquad_filter_coefficients",
        ":filter_traits",
        "//audio/dsp:porting",
        "//audio/dsp:types",
        "//third_party/eigen3",
    ],
)

cc_test(
    name = "biquad_filter_test",
    size = "small",
    srcs = ["biquad_filter_test.cc"],
    deps = [
        ":biquad_filter",
        "//audio/dsp:porting",
        "//audio/dsp:testing_util",
        "//audio/dsp:types",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_benchmark//:benchmark",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "biquad_filter_coefficients",
    srcs = ["biquad_filter_coefficients.cc"],
    hdrs = ["biquad_filter_coefficients.h"],
    deps = [
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "biquad_filter_coefficients_test",
    size = "small",
    srcs = ["biquad_filter_coefficients_test.cc"],
    deps = [
        ":biquad_filter",
        ":biquad_filter_coefficients",
        ":biquad_filter_design",
        "//audio/dsp:porting",
        "//audio/dsp:testing_util",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/strings:str_format",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "biquad_filter_design",
    srcs = [
        "biquad_filter_design.cc",
        "biquad_filter_design_pole_zero.cc",
    ],
    hdrs = ["biquad_filter_design.h"],
    deps = [
        ":biquad_filter_coefficients",
        ":discretization",
        ":filter_poles_and_zeros",
        "//audio/dsp:elliptic_functions",
        "//audio/dsp:porting",
        "//audio/dsp:signal_vector_util",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "biquad_filter_design_test",
    size = "small",
    srcs = ["biquad_filter_design_test.cc"],
    deps = [
        ":biquad_filter_design",
        ":biquad_filter_test_tools",
        "//audio/dsp:decibels",
        "//audio/dsp:porting",
        "//audio/dsp:testing_util",
        "@com_github_glog_glog//:glog",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "biquad_filter_test_tools",
    testonly = 1,
    srcs = ["biquad_filter_test_tools.cc"],
    hdrs = ["biquad_filter_test_tools.h"],
    deps = [
        ":biquad_filter_coefficients",
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@gtest//:gtest",
    ],
)

cc_test(
    name = "biquad_filter_test_tools_test",
    srcs = ["biquad_filter_test_tools_test.cc"],
    deps = [
        ":biquad_filter",
        ":biquad_filter_test_tools",
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "crossover",
    srcs = ["crossover.cc"],
    hdrs = ["crossover.h"],
    deps = [
        ":biquad_filter_coefficients",
        ":biquad_filter_design",
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "crossover_test",
    size = "small",
    srcs = ["crossover_test.cc"],
    deps = [
        ":biquad_filter_coefficients",
        ":biquad_filter_design",
        ":crossover",
        "//audio/dsp:decibels",
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/strings:str_format",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "discretization",
    srcs = ["discretization.cc"],
    hdrs = ["discretization.h"],
    deps = [
        ":biquad_filter_coefficients",
        ":filter_poles_and_zeros",
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "discretization_test",
    size = "small",
    srcs = ["discretization_test.cc"],
    deps = [
        ":discretization",
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "equalizer_filter_params",
    hdrs = ["equalizer_filter_params.h"],
    deps = [
        "//audio/dsp:porting",
    ],
)

cc_library(
    name = "filter_poles_and_zeros",
    srcs = ["filter_poles_and_zeros.cc"],
    hdrs = ["filter_poles_and_zeros.h"],
    deps = [
        ":biquad_filter_coefficients",
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "filter_poles_and_zeros_test",
    size = "small",
    srcs = ["filter_poles_and_zeros_test.cc"],
    deps = [
        ":biquad_filter_coefficients",
        ":filter_poles_and_zeros",
        "//audio/dsp:porting",
        "//audio/dsp:testing_util",
        "@com_github_glog_glog//:glog",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "filter_traits",
    hdrs = ["filter_traits.h"],
    deps = [
        "//audio/dsp:porting",
        "//audio/dsp:types",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
    ],
)

cc_library(
    name = "fir_filter",
    srcs = ["fir_filter.cc"],
    hdrs = ["fir_filter.h"],
    deps = [
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "fir_filter_test",
    size = "small",
    srcs = ["fir_filter_test.cc"],
    deps = [
        ":fir_filter",
        "//audio/dsp:testing_util",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "ladder_filter",
    srcs = ["ladder_filter-inl.h"],
    hdrs = ["ladder_filter.h"],
    deps = [
        ":biquad_filter",
        ":biquad_filter_coefficients",
        ":biquad_filter_design",
        ":filter_traits",
        "//audio/dsp:porting",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_test(
    name = "ladder_filter_test",
    size = "small",
    srcs = ["ladder_filter_test.cc"],
    deps = [
        ":biquad_filter",
        ":biquad_filter_coefficients",
        ":biquad_filter_design",
        ":ladder_filter",
        "//audio/dsp:porting",
        "//audio/dsp:testing_util",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_benchmark//:benchmark",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "parametric_equalizer",
    srcs = ["parametric_equalizer.cc"],
    hdrs = ["parametric_equalizer.h"],
    deps = [
        ":biquad_filter_coefficients",
        ":biquad_filter_design",
        ":equalizer_filter_params",
        "//audio/dsp:decibels",
        "//audio/dsp:nelder_mead_searcher",
        "//audio/dsp:porting",
        "//third_party/eigen3",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "parametric_equalizer_test",
    size = "medium",
    srcs = ["parametric_equalizer_test.cc"],
    deps = [
        ":biquad_filter_coefficients",
        ":biquad_filter_design",
        ":equalizer_filter_params",
        ":parametric_equalizer",
        "//audio/dsp:decibels",
        "//audio/dsp:porting",
        "//audio/dsp:signal_generator",
        "//audio/dsp:testing_util",
        "//third_party/eigen3",
        "@com_google_absl//absl/types:span",
        "@com_google_benchmark//:benchmark",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "perceptual_filter_design",
    srcs = ["perceptual_filter_design.cc"],
    hdrs = ["perceptual_filter_design.h"],
    deps = [
        ":biquad_filter_coefficients",
        ":discretization",
        ":filter_poles_and_zeros",
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "perceptual_filter_design_test",
    size = "small",
    srcs = ["perceptual_filter_design_test.cc"],
    deps = [
        ":perceptual_filter_design",
        "//audio/dsp:decibels",
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "two_tap_fir_filter",
    hdrs = ["two_tap_fir_filter.h"],
    deps = [
        "//audio/dsp:porting",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "two_tap_fir_filter_test",
    size = "small",
    srcs = ["two_tap_fir_filter_test.cc"],
    deps = [
        ":two_tap_fir_filter",
        "//audio/dsp:porting",
        "//audio/dsp:testing_util",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
        "@com_google_benchmark//:benchmark",
        "@gtest//:gtest_main",
    ],
)
