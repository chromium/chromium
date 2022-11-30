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

# This file contains build flags used to build MalDocA (maldoca/*).
# Those flags are not applied to build MalDocA's third party 
# dependencies (third_party/*). For chromium-specific build flags,
# see mini_chromium.BUILD.

DEFAULT_MALDOCA_WIN_COPTS = [
    "-Wno-builtin-macro-redefined",
    "-Wimplicit-fallthrough",
    "-Wunreachable-code",
    "-Wthread-safety",
    "-Wextra-semi",
    "-Wno-missing-field-initializers",
    "-Wno-unused-parameter",
    "-Wno-c++11-narrowing",
    "-Wno-unneeded-internal-declaration",
    "-Wno-undefined-var-template",
    "-Wno-nonportable-include-path",
    "-Wno-psabi",
    "-Wno-ignored-pragma-optimize",
    "-Wno-implicit-int-float-conversion",
    "-Wno-final-dtor-non-final-class",
    "-Wno-builtin-assume-aligned-alignment",
    "-Wno-deprecated-copy",
    "-Wno-non-c-typedef-for-linkage",
    "-Wmax-tokens",
    "-Wheader-hygiene",
    "-Wstring-conversion",
    "-Wtautological-overlap-compare",
    "-Wno-shorten-64-to-32",
    "-Wno-undefined-bool-conversion",
    "-Wno-tautological-undefined-compare",
    "-Wno-trigraphs",
    "-Wno-deprecated-declarations",
]

DEFAULT_MALDOCA_LINUX_COPTS = [
    "-Wno-strict-aliasing",
    "-fno-exceptions",
    "-Wall",
    "-Werror",
    "-Wextra",
    "-Wimplicit-fallthrough",
    "-Wunreachable-code",
    "-Wthread-safety",
    "-Wextra-semi",
    "-Wno-missing-field-initializers",
    "-Wno-unused-parameter",
    "-Wno-c++11-narrowing",
    "-Wno-unneeded-internal-declaration",
    "-Wno-undefined-var-template",
    "-Wno-psabi",
    "-Wno-ignored-pragma-optimize",
    "-Wno-implicit-int-float-conversion",
    "-Wno-final-dtor-non-final-class",
    "-Wno-builtin-assume-aligned-alignment",
    "-Wno-deprecated-copy",
    "-Wno-non-c-typedef-for-linkage",
    "-Wmax-tokens",
    "-Wheader-hygiene",
    "-Wstring-conversion",
    "-Wtautological-overlap-compare",
    "-Wno-sign-compare",
    "-Wno-error=unreachable-code",
    "-Wno-unused-private-field",
    "-Wno-c++98-compat-pedantic",
    "-Wno-comment",
    "-Wno-ignored-qualifiers",
    "-Wno-unused-const-variable",
    "-Wno-shorten-64-to-32",
    "-Wno-unknown-warning-option",
]

DEFAULT_MALDOCA_COPTS = select({
	"@platforms//os:linux": DEFAULT_MALDOCA_LINUX_COPTS,
	"@platforms//os:windows": DEFAULT_MALDOCA_WIN_COPTS,
})
