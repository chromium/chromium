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

licenses(["notice"])  # Apache v2.0

load("@rules_cc//cc:defs.bzl", "cc_proto_library")

package(default_visibility = ["//visibility:public"])

proto_library(
    name = "tf_example",
    srcs = [
        "tensorflow/core/example/example.proto",
    ],
    deps = [
        "@com_google_protobuf//:wrappers_proto",
        ":tf_feature"
    ],
)

cc_proto_library(
    name = "tf_example_cc_proto",
    deps = [
        ":tf_example",
    ],
)

proto_library(
    name = "tf_feature",
    srcs = [
        "tensorflow/core/example/feature.proto",
    ],
    deps = [
        "@com_google_protobuf//:wrappers_proto",
    ],
)

cc_proto_library(
    name = "tf_feature_cc_proto",
    deps = [
        ":tf_feature",
    ],
)
