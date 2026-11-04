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

# Tools pertaining to loudness measurements.

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "background_level_detector",
    srcs = ["background_level_detector.cc"],
    hdrs = ["background_level_detector.h"],
    deps = [
        "//audio/dsp:decibels",
        "//audio/dsp:eigen_types",
        "//audio/dsp:envelope_detector",
        "//audio/linear_filters:biquad_filter",
        "//audio/linear_filters:biquad_filter_design",
        "//audio/linear_filters:discretization",
        "//audio/linear_filters:ladder_filter",
        "//audio/linear_filters:perceptual_filter_design",
        "@com_github_glog_glog//:glog",
        "//third_party/eigen3",
    ],
)

cc_library(
    name = "loudness_1771_two_way_compressor",
    srcs = ["loudness_1771_two_way_compressor.cc"],
    hdrs = ["loudness_1771_two_way_compressor.h"],
    deps = [
        ":streaming_loudness_1771",
        "//audio/dsp:attack_release_envelope",
        "//audio/dsp:decibels",
        "//audio/dsp:fixed_delay_line",
        "//audio/dsp/hifi:dynamic_range_control_functions",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/memory",
        "//third_party/eigen3",
    ],
)

cc_library(
    name = "streaming_loudness_1771",
    srcs = ["streaming_loudness_1771.cc"],
    hdrs = ["streaming_loudness_1771.h"],
    deps = [
        "//audio/dsp:decibels",
        "//audio/dsp:envelope_detector",
        "//audio/linear_filters:biquad_filter",
        "//audio/linear_filters:biquad_filter_coefficients",
        "//audio/linear_filters:discretization",
        "//audio/linear_filters:perceptual_filter_design",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "//third_party/eigen3",
    ],
)

cc_test(
    name = "streaming_loudness_1771_test",
    size = "small",
    srcs = ["streaming_loudness_1771_test.cc"],
    deps = [
        ":streaming_loudness_1771",
        "//audio/dsp:decibels",
        "//audio/dsp:signal_generator",
        "@com_google_absl//absl/status:statusor",
        "@gtest//:gtest_main",
        "//third_party/eigen3",
    ],
)

cc_test(
    name = "loudness_1771_two_way_compressor_test",
    size = "small",
    srcs = ["loudness_1771_two_way_compressor_test.cc"],
    deps = [
        ":loudness_1771_two_way_compressor",
        "//audio/dsp:decibels",
        "//audio/dsp:testing_util",
        "//audio/dsp/hifi:dynamic_range_control_functions",
        "//audio/dsp/loudness:streaming_loudness_1771",
        "@gtest//:gtest_main",
        "//third_party/eigen3",
    ],
)
