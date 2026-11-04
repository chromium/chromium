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

"""Blaze support for C.
Defines c_binary, c_library, c_test rules for targets written in C. They are
like their cc_* counterparts, but compile with C89 standard compatibility.
"""

load("@rules_cc//cc:cc_binary.bzl", "cc_binary")
load("@rules_cc//cc:cc_library.bzl", "cc_library")
load("@rules_cc//cc:cc_test.bzl", "cc_test")

WARNING_OPTS =  select({
    "@bazel_tools//src/conditions:windows": [],
    "//conditions:default": [
         # Suppress "unused function" warnings on `static` functions in .h files.
         # Excluded from Windows due to lack of support by Visual Studio 2017. 
         "-Wno-unused-function",
    ]
})

# Build with C89 standard compatibility.
DEFAULT_C_OPTS = WARNING_OPTS + ["-std=c89"]

def c_binary(name = None, **kwargs):
    """cc_binary with DEFAULT_COPTS."""
    kwargs.update({"copts": DEFAULT_C_OPTS + kwargs.get("copts", [])})
    return cc_binary(name = name, **kwargs)

def c_library(name = None, **kwargs):
    """cc_library with DEFAULT_C_OPTS, and hdrs is used as textual_hrds."""
    kwargs.update({"copts": DEFAULT_C_OPTS + kwargs.get("copts", [])})

    # Use "hdrs" as "textual_hdrs". All code that cannot be standalone-compiled
    # as C++ must be listed in textual_hdrs.
    kwargs.setdefault("textual_hdrs", kwargs.pop("hdrs", None))
    return cc_library(name = name, **kwargs)

def c_test(name = None, **kwargs):
    """cc_test with DEFAULT_COPTS."""
    kwargs.update({"copts": DEFAULT_C_OPTS + kwargs.get("copts", [])})
    return cc_test(name = name, **kwargs)
