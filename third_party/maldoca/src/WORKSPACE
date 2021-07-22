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

workspace(name = "com_google_maldoca")

load("@bazel_tools//tools/build_defs/repo:git.bzl", "git_repository")
load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive", "http_jar")

# We use maybe to avoid multiple loads of the same repo.
# If the repo is already loaded, "maybe" has no effect
load("@bazel_tools//tools/build_defs/repo:utils.bzl", "maybe")

# Rules Python; This package has to be loaded in the beginning, otherwise other
# packages load an outdated version.
maybe(
    http_archive,
    name = "rules_python",
    url = "https://github.com/bazelbuild/rules_python/releases/download/0.3.0/rules_python-0.3.0.tar.gz",
    sha256 = "934c9ceb552e84577b0faf1e5a2f0450314985b4d8712b2b70717dc679fdc01b",
)

# Protobuf
maybe(
    local_repository,
    name = "com_google_protobuf",
    path = "./third_party/protobuf",
)

# C++ rules for Bazel.
maybe(
    local_repository,
    name = "rules_cc",
    path = "./third_party/rules_cc",
)

# Protobuf rules for Bazel.
maybe(
    local_repository,
    name = "rules_proto",
    path = "./third_party/rules_proto",
)

load("@rules_proto//proto:repositories.bzl", "rules_proto_dependencies", "rules_proto_toolchains")

rules_proto_dependencies()

rules_proto_toolchains()

# Build pkg
http_archive(
    name = "rules_pkg",
    sha256 = "aeca78988341a2ee1ba097641056d168320ecc51372ef7ff8e64b139516a4937",
    urls = [
        "https://github.com/bazelbuild/rules_pkg/releases/download/0.2.6-1/rules_pkg-0.2.6.tar.gz",
        "https://mirror.bazel.build/github.com/bazelbuild/rules_pkg/releases/download/0.2.6/rules_pkg-0.2.6.tar.gz",
    ],
)

load("@rules_pkg//:deps.bzl", "rules_pkg_dependencies")

rules_pkg_dependencies()

# Abseil C++
maybe(
    local_repository,
    name = "com_google_absl",
    path = "./third_party/abseil-cpp",
)

# Google Test
maybe(
    local_repository,
    name = "com_google_googletest",
    path = "./third_party/googletest",
)

# Google RE2 (Regular Expression) C++ Library
maybe(
    local_repository,
    name = "com_googlesource_code_re2",
    path = "./third_party/re2",
)

# Benchmark
maybe(
    local_repository,
    name = "com_google_benchmark",
    path = "./third_party/benchmark",
)

# tensorflow - We only care about example.proto and feature.proto
maybe(
    new_local_repository,
    name = "tensorflow_protos",
    build_file = "@//:bazel/tensorflow_protos.BUILD",
    path = "./third_party/tensorflow_protos",
)

# ZetaSQL - We only care about //zetasql/base/...
maybe(
    new_local_repository,
    name = "com_google_zetasql",
    build_file = "@//:bazel/zetasql.BUILD",
    path = "third_party/zetasql",
)

# Zlib
maybe(
    new_local_repository,
    name = "zlib",
    build_file = "@com_google_protobuf//:third_party/zlib.BUILD",
    path = "./third_party/zlib",
)

# Zlib's minizip
maybe(
    new_local_repository,
    name = "minizip",
    build_file = "@//:bazel/minizip.BUILD",
    path = "./third_party/zlib",
)

# zip_reader from zlib in Chromium's third party directory.
maybe(
    new_local_repository,
    name = "zip_reader",
    build_file = "@//:bazel/zip_reader.BUILD",
    path = "./third_party/chromium",
)

# Zlibwrapper
maybe(
    new_local_repository,
    name = "zlibwrapper",
    build_file = "@//:bazel/zlibwrapper.BUILD",
    path = "./third_party/zlibwrapper",
)

# Libxml
maybe(
    new_local_repository,
    name = "libxml",
    build_file = "@//:bazel/libxml.BUILD",
    path = "./third_party/libxml2",
)

# Libxml configs
maybe(
    new_local_repository,
    name = "libxml_config",
    build_file = "@//:third_party/libxml2_config/BUILD.bazel",
    path = "./third_party/libxml2_config",
)

# BoringSSL
maybe(
    local_repository,
    name = "boringssl",
    path = "./third_party/boringssl",
)

# gRPC. Note it needs to be named com_github_grpc_grpc because it's referred
# as such internally.
local_repository(
    name = "com_github_grpc_grpc",
    path = "./third_party/grpc",
)

load("@com_github_grpc_grpc//bazel:grpc_deps.bzl", "grpc_deps")

grpc_deps()

# Not mentioned in official docs... mentioned here https://github.com/grpc/grpc/issues/20511
load("@com_github_grpc_grpc//bazel:grpc_extra_deps.bzl", "grpc_extra_deps")

grpc_extra_deps()

# Need to separately setup Protobuf dependencies in order for the build rules
# to work.
load("@com_google_protobuf//:protobuf_deps.bzl", "protobuf_deps")

protobuf_deps()

# Mini Chromium
maybe(
    new_local_repository,
    name = "mini_chromium",
    build_file = "@//:bazel/mini_chromium.BUILD",
    path = "./third_party/mini_chromium",
)
