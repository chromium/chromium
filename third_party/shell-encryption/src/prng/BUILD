# Copyright 2019 Google LLC.
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

package(default_visibility = ["//visibility:public"])

licenses(["notice"])

exports_files(["LICENSE"])

# PRNG interface.

cc_library(
    name = "prng",
    hdrs = ["prng.h"],
    deps = [
        "//:integral_types",
        "//:statusor_fork",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "integral_prng_types",
    hdrs = ["integral_prng_types.h"],
    deps = [
        "//prng:chacha_prng",
        "//prng:single_thread_chacha_prng",
    ],
)

cc_library(
    name = "integral_prng_testing_types",
    testonly = 1,
    hdrs = ["integral_prng_testing_types.h"],
    deps = [
        ":chacha_prng",
        ":single_thread_chacha_prng",
        "@com_github_google_googletest//:gtest",
    ],
)

# PRNG test common to all PRNGs implementations.

cc_test(
    name = "prng_test",
    size = "small",
    srcs = [
        "prng_test.cc",
    ],
    deps = [
        ":integral_prng_testing_types",
        ":integral_prng_types",
        "//testing:matchers",
        "//testing:status_testing",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
    ],
)

# PRNG using ChaCha20 from OpenSSL.

cc_library(
    name = "chacha_prng",
    srcs = [
        "chacha_prng.cc",
    ],
    hdrs = ["chacha_prng.h"],
    deps = [
        ":chacha_prng_util",
        ":prng",
        "//:statusor_fork",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/synchronization",
    ],
)

cc_library(
    name = "single_thread_chacha_prng",
    srcs = ["single_thread_chacha_prng.cc"],
    hdrs = ["single_thread_chacha_prng.h"],
    deps = [
        ":chacha_prng_util",
        ":prng",
        "//:statusor_fork",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "chacha_prng_util",
    srcs = [
        "chacha_prng_util.cc",
    ],
    hdrs = ["chacha_prng_util.h"],
    deps = [
        "//:integral_types",
        "//:statusor_fork",
        "@boringssl//:ssl",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "chacha_prng_test",
    size = "small",
    srcs = [
        "chacha_prng_test.cc",
    ],
    deps = [
        ":chacha_prng",
        "//testing:matchers",
        "//testing:status_testing",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "single_thread_chacha_prng_test",
    size = "small",
    srcs = [
        "single_thread_chacha_prng_test.cc",
    ],
    deps = [
        ":single_thread_chacha_prng",
        "//testing:matchers",
        "//testing:status_testing",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/strings",
    ],
)
