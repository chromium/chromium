# Copyright 2017 Google LLC.
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

load("@rules_cc//cc:defs.bzl", "cc_library")
load("@rules_proto//proto:defs.bzl", "proto_library")

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

exports_files(["LICENSE"])

# Utilities.

cc_library(
    name = "parameters",
    testonly = 1,
    hdrs = ["parameters.h"],
    deps = [
        "//:constants",
        "//:context",
        "//:integral_types",
        "//:montgomery",
        "@com_github_google_googletest//:gtest",
        "@com_google_absl//absl/numeric:int128",
    ],
)

cc_library(
    name = "testing_prng",
    testonly = 1,
    hdrs = ["testing_prng.h"],
    deps = [
        "//:integral_types",
        "//prng",
    ],
)

cc_library(
    name = "status_testing",
    testonly = 1,
    hdrs = ["status_testing.h"],
    deps = ["//:statusor_fork"],
)

cc_library(
    name = "testing_utils",
    testonly = 1,
    hdrs = ["testing_utils.h"],
    deps = [
        ":testing_prng",
        "//:constants",
        "//:montgomery",
        "//:polynomial",
        "//:statusor_fork",
        "//:symmetric_encryption",
    ],
)

cc_library(
    name = "status_is_fork",
    testonly = 1,
    hdrs = [
        "protobuf_matchers.h",
        "status_matchers.h",
    ],
    deps = [
        "//:statusor_fork",
        "@com_github_google_googletest//:gtest",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "matchers",
    testonly = 1,
    hdrs = [
        "protobuf_matchers.h",
        "status_matchers.h",
    ],
    deps = [
        "//:statusor_fork",
        "@com_github_google_googletest//:gtest",
        "@com_google_absl//absl/status",
    ],
)

cc_test(
    name = "protobuf_matchers_test",
    srcs = ["protobuf_matchers_test.cc"],
    deps = [
        ":coefficient_polynomial_cc_proto",
        ":matchers",
        ":status_testing",
        "@com_github_google_googletest//:gtest_main",
    ],
)

# Polynomials represented by their coefficient vector.

cc_library(
    name = "coefficient_polynomial",
    testonly = 1,
    hdrs = ["coefficient_polynomial.h"],
    deps = [
        ":coefficient_polynomial_cc_proto",
        "//:statusor_fork",
        "@com_github_google_glog//:glog",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "coefficient_polynomial_test",
    srcs = [
        "coefficient_polynomial_test.cc",
    ],
    deps = [
        ":coefficient_polynomial",
        ":status_is_fork",
        ":status_testing",
        ":testing_prng",
        "//:constants",
        "//:montgomery",
        "@com_github_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "coefficient_polynomial_ciphertext",
    testonly = 1,
    hdrs = ["coefficient_polynomial_ciphertext.h"],
    deps = [
        ":coefficient_polynomial",
        "//:error_params",
        "//:polynomial",
        "//:statusor_fork",
        "//:symmetric_encryption",
    ],
)

cc_test(
    name = "coefficient_polynomial_ciphertext_test",
    srcs = [
        "coefficient_polynomial_ciphertext_test.cc",
    ],
    deps = [
        ":coefficient_polynomial",
        ":coefficient_polynomial_ciphertext",
        ":status_is_fork",
        ":status_testing",
        ":testing_prng",
        ":testing_utils",
        "//:constants",
        "//:montgomery",
        "//:ntt_parameters",
        "//:polynomial",
        "//:symmetric_encryption",
        "@com_github_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "coefficient_polynomial_proto",
    srcs = ["coefficient_polynomial.proto"],
)

cc_proto_library(
    name = "coefficient_polynomial_cc_proto",
    deps = [":coefficient_polynomial_proto"],
)
