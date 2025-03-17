# Description:
#   Utilities for building linear filterbanks.

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "auditory_cascade_filterbank",
    srcs = ["auditory_cascade_filterbank.cc"],
    hdrs = ["auditory_cascade_filterbank.h"],
    deps = [
        ":auditory_cascade_filterbank_params_proto",
        "//audio/dsp:porting",
        "//audio/linear_filters:biquad_filter",
        "//audio/linear_filters:biquad_filter_coefficients",
        "//audio/linear_filters:biquad_filter_design",
        "//audio/linear_filters:discretization",
        "//audio/linear_filters:two_tap_fir_filter",
        "//audio/linear_filters/filterbanks:factor_two_decimator",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "auditory_cascade_filterbank_test",
    size = "small",
    srcs = ["auditory_cascade_filterbank_test.cc"],
    deps = [
        ":auditory_cascade_filterbank",
        ":auditory_cascade_filterbank_params_proto",
        "//audio/dsp:porting",
        "//audio/dsp:testing_util",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/strings:str_format",
        "@gtest//:gtest_main",
    ],
)

cc_proto_library(
    name = "auditory_cascade_filterbank_params_proto",
    deps = [":auditory_cascade_filterbank_params_generic_proto"],
)

proto_library(
    name = "auditory_cascade_filterbank_params_generic_proto",
    srcs = ["auditory_cascade_filterbank_params.proto"],
)

cc_library(
    name = "factor_two_decimator",
    hdrs = ["factor_two_decimator.h"],
    deps = [
        "//audio/dsp:porting",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "factor_two_decimator_test",
    size = "small",
    srcs = ["factor_two_decimator_test.cc"],
    deps = [
        ":factor_two_decimator",
        "//audio/dsp:porting",
        "//audio/dsp:testing_util",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
        "@gtest//:gtest_main",
    ],
)
