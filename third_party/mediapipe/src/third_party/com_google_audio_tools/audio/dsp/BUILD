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

# Description:
#   DSP tools for processing audio.

load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "porting",
    srcs = ["porting.cc"],
    # Files get moved into audio/dsp by copy.bara.sky.
    hdrs = ["porting.h"],
    deps = [
        "@com_google_absl//absl/log:die_if_null",
    ],
)

cc_library(
    name = "attack_release_envelope",
    srcs = ["attack_release_envelope.cc"],
    hdrs = ["attack_release_envelope.h"],
    deps = [
        ":porting",
        "//audio/linear_filters:biquad_filter",
        "//audio/linear_filters:biquad_filter_coefficients",
        "//audio/linear_filters:biquad_filter_design",
        "//audio/linear_filters:discretization",
    ],
)

cc_test(
    name = "attack_release_envelope_test",
    size = "small",
    srcs = ["attack_release_envelope_test.cc"],
    deps = [
        ":attack_release_envelope",
        "//audio/linear_filters:discretization",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "bessel_functions",
    srcs = ["bessel_functions.cc"],
    hdrs = ["bessel_functions.h"],
    deps = [
        ":porting",
        ":signal_vector_util",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "bessel_functions_test",
    size = "small",
    srcs = ["bessel_functions_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":bessel_functions",
        ":porting",
        ":signal_vector_util",
        "@com_google_absl//absl/strings",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "batch_top_n",
    hdrs = ["batch_top_n.h"],
    deps = [
        ":porting",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "batch_top_n_test",
    size = "small",
    srcs = ["batch_top_n_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":batch_top_n",
        ":testing_util",
        "//third_party/eigen3",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "circular_buffer",
    hdrs = ["circular_buffer.h"],
    deps = [
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "circular_buffer_test",
    size = "small",
    srcs = ["circular_buffer_test.cc"],
    deps = [
        ":circular_buffer",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/types:span",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "decibels",
    hdrs = ["decibels.h"],
    deps = [
        ":porting",
        "//third_party/eigen3",
    ],
)

cc_test(
    name = "decibels_test",
    size = "small",
    srcs = ["decibels_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":decibels",
        ":porting",
        ":testing_util",
        "//third_party/eigen3",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "eigen_types",
    hdrs = ["eigen_types.h"],
    deps = [
        ":porting",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:nullability",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "eigen_types_test",
    size = "small",
    srcs = ["eigen_types_test.cc"],
    deps = [
        ":eigen_types",
        ":porting",
        ":testing_util",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "elliptic_functions",
    srcs = ["elliptic_functions.cc"],
    hdrs = ["elliptic_functions.h"],
    deps = [
        ":porting",
        ":signal_vector_util",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "elliptic_functions_test",
    size = "small",
    srcs = ["elliptic_functions_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":elliptic_functions",
        ":porting",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "envelope_detector",
    srcs = ["envelope_detector.cc"],
    hdrs = ["envelope_detector.h"],
    deps = [
        ":porting",
        ":resampler_q",
        "//audio/linear_filters:biquad_filter",
        "//audio/linear_filters:biquad_filter_design",
        "//third_party/eigen3",
    ],
)

cc_test(
    name = "envelope_detector_test",
    srcs = ["envelope_detector_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":envelope_detector",
        ":porting",
        ":testing_util",
        "//audio/linear_filters:biquad_filter_design",
        "//third_party/eigen3",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "fixed_delay_line",
    hdrs = ["fixed_delay_line.h"],
    deps = [
        ":porting",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "fixed_delay_line_test",
    srcs = ["fixed_delay_line_test.cc"],
    deps = [
        ":fixed_delay_line",
        ":porting",
        ":testing_util",
        "//third_party/eigen3",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "kiss_fft",
    srcs = ["kiss_fft.cc"],
    hdrs = ["kiss_fft.h"],
    deps = [
        ":porting",
        "//third_party/kissfft:kiss_fft_float",
        "@com_github_glog_glog//:glog",
    ],
)

cc_library(
    name = "kiss_fft_util",
    srcs = ["kiss_fft_util.cc"],
    hdrs = ["kiss_fft_util.h"],
    deps = [
        ":eigen_types",
        ":kiss_fft",
        ":porting",
        ":signal_vector_util",
        "//third_party/eigen3",
    ],
)

cc_library(
    name = "max_filter",
    hdrs = ["max_filter.h"],
    deps = [
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "max_filter_test",
    size = "small",
    srcs = ["max_filter_test.cc"],
    deps = [
        ":max_filter",
        "@com_google_absl//absl/strings:str_format",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "nelder_mead_searcher",
    hdrs = ["nelder_mead_searcher.h"],
    deps = [
        ":porting",
        "//third_party/eigen3",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "nelder_mead_searcher_test",
    srcs = ["nelder_mead_searcher_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":nelder_mead_searcher",
        ":porting",
        ":testing_util",
        "//third_party/eigen3",
        "@com_google_benchmark//:benchmark",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "number_util",
    srcs = ["number_util.cc"],
    hdrs = ["number_util.h"],
    deps = [
        ":porting",
        "//audio/dsp/portable:number_util",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "number_util_test",
    size = "small",
    srcs = ["number_util_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":number_util",
        ":porting",
        ":testing_util",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "resampler",
    hdrs = ["resampler.h"],
    deps = [
        ":porting",
    ],
)

cc_library(
    name = "resampler_q",
    srcs = ["resampler_q.cc"],
    hdrs = ["resampler_q.h"],
    deps = [
        ":eigen_types",
        ":number_util",
        ":porting",
        ":resampler",
        ":types",
        "//audio/dsp/portable:rational_factor_resampler_kernel",
        "//third_party/eigen3",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "resampler_q_test",
    size = "large",
    srcs = [
        "eigen_types.h",
        "resampler_q.cc",
        "resampler_q.h",
        "resampler_q_test.cc",
        "types.h",
    ],
    copts = ["-Iexternal/gtest/include"],
    defines = ["EIGEN_RUNTIME_NO_MALLOC"],  # Test Eigen memory allocations.
    deps = [
        ":number_util",
        ":resampler",
        ":signal_vector_util",
        ":testing_util",
        "//audio/dsp/portable:rational_factor_resampler_kernel",
        "//third_party/eigen3",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "resampler_rational_factor",
    srcs = ["resampler_rational_factor.cc"],
    hdrs = ["resampler_rational_factor.h"],
    deprecation = "RationalFactorResampler is superseded by QResampler. " +
                  "Please see migration notes at the top of resampler_rational_factor.h.",
    deps = [
        ":bessel_functions",
        ":number_util",
        ":porting",
        ":resampler",
        ":types",
        "//third_party/eigen3",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "resampler_rational_factor_test",
    size = "large",
    srcs = ["resampler_rational_factor_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":porting",
        ":resampler_rational_factor",
        ":signal_vector_util",
        ":testing_util",
        ":types",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_benchmark//:benchmark",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "signal_eigen_util",
    hdrs = ["signal_eigen_util.h"],
    deps = [
        ":batch_top_n",
        ":porting",
        "//third_party/eigen3",
    ],
)

cc_test(
    name = "signal_eigen_util_test",
    size = "small",
    srcs = ["signal_eigen_util_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":porting",
        ":signal_eigen_util",
        ":testing_util",
        "//third_party/eigen3",
        "@com_google_benchmark//:benchmark",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "signal_generator",
    srcs = ["signal_generator.cc"],
    hdrs = ["signal_generator.h"],
    deps = [
        ":porting",
        "//audio/linear_filters:biquad_filter",
        "//audio/linear_filters:biquad_filter_coefficients",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "signal_generator_test",
    size = "small",
    srcs = ["signal_generator_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":porting",
        ":signal_generator",
        ":testing_util",
        ":types",
        "//third_party/eigen3",
        "@com_google_absl//absl/strings:str_format",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "signal_vector_util",
    hdrs = ["signal_vector_util.h"],
    deps = [
        ":porting",
        ":types",
    ],
)

cc_library(
    name = "spectral_processor",
    srcs = ["spectral_processor.cc"],
    hdrs = ["spectral_processor.h"],
    deps = [
        ":circular_buffer",
        ":kiss_fft",
        ":number_util",
        ":window_functions",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "spectral_processor_test",
    size = "small",
    srcs = ["spectral_processor_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":signal_generator",
        ":spectral_processor",
        ":testing_util",
        ":window_functions",
        "//third_party/eigen3",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "teager_energy_filter",
    hdrs = ["teager_energy_filter.h"],
    deps = [
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "teager_energy_filter_test",
    size = "small",
    srcs = ["teager_energy_filter_test.cc"],
    deps = [
        ":signal_vector_util",
        ":teager_energy_filter",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "testing_util",
    testonly = 1,
    hdrs = ["testing_util.h"],
    deps = [
        ":porting",
        "//third_party/eigen3",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/types:span",
        "@gtest",
    ],
)

cc_test(
    name = "testing_util_test",
    size = "small",
    srcs = ["testing_util_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":porting",
        ":testing_util",
        "@com_github_glog_glog//:glog",
        "@com_google_absl//absl/types:span",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "types",
    hdrs = ["types.h"],
    deps = [
        ":porting",
    ],
)

cc_test(
    name = "types_test",
    size = "small",
    srcs = ["types_test.cc"],
    copts = ["-Iexternal/gtest/include"],
    deps = [
        ":porting",
        ":types",
        "@com_google_absl//absl/memory",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "window_functions",
    srcs = ["window_functions.cc"],
    hdrs = ["window_functions.h"],
    deps = [
        ":bessel_functions",
        ":porting",
    ],
)

cc_test(
    name = "window_functions_test",
    size = "small",
    srcs = ["window_functions_test.cc"],
    deps = [
        ":kiss_fft",
        ":signal_vector_util",
        ":testing_util",
        ":window_functions",
        "@com_google_absl//absl/strings",
        "@gtest//:gtest_main",
    ],
)
