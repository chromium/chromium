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
    name = "mfcc",
    srcs = [
        "dct.cc",
        "mel_filterbank.cc",
        "mfcc.cc",
    ],
    hdrs = [
        "dct.h",
        "mel_filterbank.h",
        "mfcc.h",
    ],
    deps = [
        "//audio/dsp:porting",
        "@com_github_glog_glog//:glog",
    ],
)

cc_test(
    name = "mel_filterbank_test",
    size = "small",
    srcs = ["mel_filterbank_test.cc"],
    deps = [
        ":mfcc",
        "//audio/dsp:porting",
        "//audio/dsp:signal_vector_util",
        "@com_google_absl//absl/random",
        "@gtest//:gtest_main",

    ],
)

cc_test(
    name = "dct_test",
    size = "small",
    srcs = ["dct_test.cc"],
    deps = [
        ":mfcc",
        "//audio/dsp:porting",
        "@gtest//:gtest_main",
    ],
)

cc_test(
    name = "mfcc_test",
    size = "small",
    srcs = ["mfcc_test.cc"],
    deps = [
        ":mfcc",
        "//audio/dsp:porting",
        "//audio/dsp:signal_vector_util",
        "@com_github_glog_glog//:glog",
        "@gtest//:gtest_main",
    ],
)
