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

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "inverse_spectrogram",
    srcs = ["inverse_spectrogram.cc"],
    hdrs = ["inverse_spectrogram.h"],
    deps = [
        "//audio/dsp:number_util",
        "//third_party/eigen3:eigen3",
        "//third_party/fft2d",
        "//third_party/pffft:pffft",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "inverse_spectrogram_test",
    size = "small",
    srcs = ["inverse_spectrogram_test.cc"],
    data = [
        "//audio/dsp/spectrogram/testdata:short_test_segment.wav",
        "//audio/dsp/spectrogram/testdata:short_test_segment_reconstructed.wav",
        "//audio/dsp/spectrogram/testdata:short_test_segment_reconstructed_hann.wav",
        "//audio/dsp/spectrogram/testdata:short_test_segment_spectrogram.csv",
        "//audio/dsp/spectrogram/testdata:short_test_segment_spectrogram_1024_fft.csv",
        "//audio/dsp/spectrogram/testdata:short_test_segment_spectrogram_hann.csv",
    ],
    deps = [
        ":inverse_spectrogram",
        ":spectrogram",
        ":test_file_utils",
        "//audio/dsp:window_functions",
        "//third_party/eigen3:eigen3",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log:check",
        "@com_github_glog_glog//:glog",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "spectrogram",
    srcs = ["spectrogram.cc"],
    hdrs = ["spectrogram.h"],
    deps = [
        "//audio/dsp:number_util",
        "//audio/dsp:porting",
        "//audio/dsp:window_functions",
        "//third_party/eigen3:eigen3",
        "//third_party/fft2d:fft2d",
        "//third_party/pffft:pffft",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "spectrogram_test",
    size = "small",
    srcs = ["spectrogram_test.cc"],
    data = [
        "//audio/dsp/spectrogram/testdata:short_test_segment.wav",
        "//audio/dsp/spectrogram/testdata:short_test_segment_spectrogram.csv",
        "//audio/dsp/spectrogram/testdata:short_test_segment_spectrogram_1024_fft.csv",
        "//audio/dsp/spectrogram/testdata:short_test_segment_spectrogram_400_200.csv",
        "//audio/dsp/spectrogram/testdata:short_test_segment_spectrogram_hann.csv",
    ],
    deps = [
        ":spectrogram",
        ":test_file_utils",
        "//audio/dsp:window_functions",
        "//third_party/eigen3:eigen3",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/log:check",
        "@com_github_glog_glog//:glog",
        "@gtest//:gtest_main",
    ],
)

cc_library(
    name = "test_file_utils",
    srcs = ["test_file_utils.cc"],
    hdrs = ["test_file_utils.h"],
    deps = [
        "//audio/dsp/portable:read_wav_file",
        "@com_google_absl//absl/log:check",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
        "@com_github_glog_glog//:glog",
    ],
)
