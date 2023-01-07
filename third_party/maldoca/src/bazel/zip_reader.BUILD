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

licenses(["notice"])

package(
    default_visibility = ["//visibility:public"],
)

cc_library(
    name = "zip_reader",
    srcs = ["third_party/zlib/google/zip_reader.cc"],
    hdrs = ["third_party/zlib/google/zip_reader.h"],
    deps = [
        "@com_google_absl//absl/strings",
        "@mini_chromium//:base",
        "@minizip",
    ],
)
