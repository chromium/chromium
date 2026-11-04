# Copyright 2020-2026 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#     https://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "dynamic_range_control_functions",
    srcs = ["dynamic_range_control_functions.cc"],
    hdrs = ["dynamic_range_control_functions.h"],
    deps = [
        "@com_github_glog_glog//:glog",
        "//third_party/eigen3",
    ],
)

cc_test(
    name = "dynamic_range_control_functions_test",
    size = "small",
    srcs = ["dynamic_range_control_functions_test.cc"],
    deps = [
        ":dynamic_range_control_functions",
        "//audio/dsp:testing_util",
        "//third_party/eigen3",
        "@gtest//:gtest_main",
    ],
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
        "//audio/linear_filters:biquad_filter_coefficients",
        "//audio/linear_filters:biquad_filter_design",
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

cc_test(
    name = "multi_crossover_filter_test",
    size = "large",
    srcs = ["multi_crossover_filter_test.cc"],
    deps = [
        ":multi_crossover_filter",
        "//audio/dsp:kiss_fft",
        "//audio/dsp:max_filter",
        "//audio/dsp:testing_util",
        "//third_party/eigen3",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "multiband_compressor_test",
    size = "small",
    srcs = ["multiband_compressor_test.cc"],
    deps = [
        ":dynamic_range_control",
        ":multiband_compressor",
        "//audio/dsp:kiss_fft",
        "//audio/dsp:testing_util",
        "//third_party/eigen3",
        "@gtest//:gtest_main",
    ],
)
