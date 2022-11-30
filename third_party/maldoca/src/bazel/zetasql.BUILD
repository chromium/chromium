# Copyright 2021 Google LLC
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

# Bazel build file for third_party/zetasql/base.

load("@rules_cc//cc:defs.bzl", "cc_proto_library")

licenses(["notice"])  # Apache v2.0

package(default_visibility = ["//visibility:public"])

cc_library(
    name = "cleanup",
    hdrs = [":zetasql/base/cleanup.h"],
    copts = ["-Wno-sign-compare"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_test(
    name = "cleanup_test",
    srcs = [":zetasql/base/cleanup_test.cc"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":cleanup",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_proto_library(
    name = "descriptor_cc_proto",
    deps = [
        "@com_google_protobuf//:descriptor_proto",
    ]
)


cc_library(
    name = "endian",
    hdrs = [":zetasql/base/endian.h"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":unaligned_access",
        "@com_google_absl//absl/base:core_headers",
    ],
)

cc_library(
    name = "enum_utils",
    hdrs = [":zetasql/base/enum_utils.h"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":descriptor_cc_proto",
    ],
)

cc_test(
    name = "enum_utils_test",
    srcs = [":zetasql/base/enum_utils_test.cc"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":enum_utils",
        ":test_payload_cc_proto",
        "@com_google_protobuf//:protobuf",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "logging",
    srcs = [":zetasql/base/logging.cc"],
    hdrs = [":zetasql/base/logging.h"],
    copts = ["-Wno-sign-compare"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:log_severity",
    ],
)

cc_library(
    name = "source_location",
    hdrs = [":zetasql/base/source_location.h"],
    deps = ["@com_google_absl//absl/base:config"],
)

cc_test(
    name = "source_location_test",
    srcs = [":zetasql/base/source_location_test.cc"],
    deps = [
        ":source_location",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "ret_check",
    srcs = [":zetasql/base/ret_check.cc"],
    hdrs = [":zetasql/base/ret_check.h"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":logging",
        ":source_location",
        ":status",
        "@com_google_absl//absl/status",
    ],
)

cc_library(
    name = "status",
    srcs = [
        ":zetasql/base/status_builder.cc",
        ":zetasql/base/status_payload.cc",
        ":zetasql/base/statusor.cc",
    ],
    hdrs = [
        ":zetasql/base/canonical_errors.h",
        ":zetasql/base/status.h",
        ":zetasql/base/status_builder.h",
        ":zetasql/base/status_macros.h",
        ":zetasql/base/status_payload.h",
        ":zetasql/base/statusor.h",
        ":zetasql/base/statusor_internals.h",
    ],
    copts = ["-Wno-sign-compare"],
    visibility = ["//visibility:public"],
    deps = [
        ":logging",
        ":source_location",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/base:log_severity",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/utility",
    ],
)

cc_test(
    name = "status_builder_test",
    srcs = [":zetasql/base/status_builder_test.cc"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":source_location",
        ":status",
        ":status_matchers",
        ":test_payload_cc_proto",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "status_macros_test",
    srcs = [":zetasql/base/status_macros_test.cc"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":source_location",
        ":status",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_proto_library(
    name = "any_cc_proto",
    deps = [
        "@com_google_protobuf//:any_proto",
    ],
)

cc_test(
    name = "status_payload_test",
    srcs = [":zetasql/base/status_payload_test.cc"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":any_cc_proto",
        ":status",
        ":test_payload_cc_proto",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest_main",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_test(
    name = "status_test",
    srcs = [":zetasql/base/status_test.cc"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":status",
        "@com_google_googletest//:gtest_main",
    ],
)

proto_library(
    name = "test_payload_proto",
    testonly = 1,
    srcs = [":zetasql/base/test_payload.proto"],
)

cc_proto_library(
    name = "test_payload_cc_proto",
    testonly = 1,
    deps = [":test_payload_proto"],
)

cc_library(
    name = "status_matchers",
    testonly = 1,
    srcs = [":zetasql/base/testing/status_matchers.cc"],
    hdrs = [":zetasql/base/testing/status_matchers.h"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":source_location",
        ":status",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
    ],
)

cc_test(
    name = "status_matchers_test",
    size = "small",
    srcs = [":zetasql/base/testing/status_matchers_test.cc"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":status",
        ":status_matchers",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "statusor_test",
    size = "small",
    srcs = [":zetasql/base/statusor_test.cc"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":source_location",
        ":status",
        ":status_matchers",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "no_destructor",
    hdrs = [":zetasql/base/no_destructor.h"],
    copts = ["-Wno-sign-compare"],
)

cc_library(
    name = "map_util",
    hdrs = [
        ":zetasql/base/map_traits.h",
        ":zetasql/base/map_util.h",
    ],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":logging",
        ":no_destructor",
        "@com_google_absl//absl/meta:type_traits",
    ],
)

cc_library(
    name = "path",
    srcs = [":zetasql/base/path.cc"],
    hdrs = [":zetasql/base/path.h"],
    copts = ["-Wno-sign-compare"],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "path_test",
    srcs = [":zetasql/base/path_test.cc"],
    copts = ["-Wno-sign-compare"],
    deps = [
        ":path",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "unaligned_access",
    hdrs = [":zetasql/base/unaligned_access.h"],
    copts = ["-Wno-sign-compare"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
    ],
)
