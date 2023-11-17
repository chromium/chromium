# Copyright 2023 Google LLC
#
# Licensed under the Apache License, Version 2.0 (the "License");
# you may not use this file except in compliance with the License.
# You may obtain a copy of the License at
#
#      http://www.apache.org/licenses/LICENSE-2.0
#
# Unless required by applicable law or agreed to in writing, software
# distributed under the License is distributed on an "AS IS" BASIS,
# WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
# See the License for the specific language governing permissions and
# limitations under the License.

load("@io_bazel_rules_go//proto:def.bzl", "go_proto_library")
load("@com_github_google_iree//build_tools/embed_data:build_defs.bzl", "cc_embed_data")

package(
    default_visibility = ["//:__subpackages__"],
)

licenses(["notice"])

cc_library(
    name = "value_type_helpers",
    srcs = ["value_type_helpers.cc"],
    hdrs = ["value_type_helpers.h"],
    deps = [
        "//dpf:distributed_point_function_cc_proto",
        "//dpf:int_mod_n",
        "//dpf:status_macros",
        "//dpf:tuple",
        "//dpf:xor_wrapper",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/utility",
        "@com_google_protobuf//:protobuf_lite",
    ],
)

cc_test(
    name = "value_type_helpers_test",
    srcs = ["value_type_helpers_test.cc"],
    deps = [
        ":status_matchers",
        ":value_type_helpers",
        "//dpf:distributed_point_function_cc_proto",
        "//dpf:int_mod_n",
        "//dpf:tuple",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "status_matchers",
    testonly = 1,
    srcs = [
        "status_matchers.cc",
    ],
    hdrs = ["status_matchers.h"],
    deps = [
        "//dpf:status_macros",
        "@com_github_google_googletest//:gtest",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "proto_validator",
    srcs = [
        "proto_validator.cc",
    ],
    hdrs = [
        "proto_validator.h",
    ],
    deps = [
        ":value_type_helpers",
        "//dpf:distributed_point_function_cc_proto",
        "//dpf:status_macros",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_absl//absl/types:span",
        "@com_google_protobuf//:protobuf_lite",
    ],
)

cc_embed_data(
    name = "proto_validator_test_textproto_embed",
    srcs = [
        "proto_validator_test.textproto",
    ],
    cc_file_output = "proto_validator_test_textproto_embed.cc",
    cpp_namespace = "distributed_point_functions::dpf_internal",
    h_file_output = "proto_validator_test_textproto_embed.h",
)

cc_test(
    name = "proto_validator_test",
    srcs = [
        "proto_validator_test.cc",
    ],
    data = [
        "proto_validator_test.textproto",
    ],
    deps = [
        ":proto_validator",
        ":proto_validator_test_textproto_embed",
        ":status_matchers",
        "//dpf:distributed_point_function_cc_proto",
        "//dpf:tuple",
        "@com_github_google_googletest//:gtest_main",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_protobuf//:protobuf",
    ],
)

cc_library(
    name = "evaluate_prg_hwy",
    srcs = ["evaluate_prg_hwy.cc"],
    hdrs = ["evaluate_prg_hwy.h"],
    deps = [
        ":aes_128_fixed_key_hash_hwy",
        "//dpf:aes_128_fixed_key_hash",
        "//dpf:status_macros",
        "@boringssl//:crypto",
        "@com_github_google_highway//:hwy",
        "@com_google_absl//absl/base:config",
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/log:absl_check",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "evaluate_prg_hwy_test",
    srcs = [
        "evaluate_prg_hwy_test.cc",
    ],
    deps = [
        ":evaluate_prg_hwy",
        ":status_matchers",
        "//dpf:aes_128_fixed_key_hash",
        "@com_github_google_googletest//:gtest_main",
        "@com_github_google_highway//:hwy",
        "@com_github_google_highway//:hwy_test_util",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
    ],
)

cc_library(
    name = "get_hwy_mode",
    srcs = ["get_hwy_mode.cc"],
    hdrs = ["get_hwy_mode.h"],
    deps = [
        "@com_github_google_highway//:hwy",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "aes_128_fixed_key_hash_hwy",
    hdrs = [
        "aes_128_fixed_key_hash_hwy.h",
    ],
    deps = [
        "@com_github_google_highway//:hwy",
        "@com_google_absl//absl/numeric:int128",
    ],
)

cc_library(
    name = "maybe_deref_span",
    hdrs = ["maybe_deref_span.h"],
    deps = [
        "@com_google_absl//absl/meta:type_traits",
        "@com_google_absl//absl/types:span",
        "@com_google_absl//absl/types:variant",
    ],
)

cc_test(
    name = "aes_128_fixed_key_hash_hwy_test",
    srcs = [
        "aes_128_fixed_key_hash_hwy_test.cc",
    ],
    deps = [
        ":aes_128_fixed_key_hash_hwy",
        ":get_hwy_mode",
        ":status_matchers",
        "//dpf:aes_128_fixed_key_hash",
        "@boringssl//:crypto",
        "@com_github_google_googletest//:gtest_main",
        "@com_github_google_highway//:hwy",
        "@com_github_google_highway//:hwy_test_util",
        "@com_google_absl//absl/flags:parse",
        "@com_google_absl//absl/log:absl_log",
        "@com_google_absl//absl/numeric:int128",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/types:span",
    ],
)

cc_test(
    name = "maybe_deref_span_test",
    srcs = ["maybe_deref_span_test.cc"],
    deps = [
        ":maybe_deref_span",
        "@com_github_google_googletest//:gtest_main",
    ],
)
