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

DEFAULT_MINI_CHROMIUM_BASE_COPTS = [
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
    "-Wno-shorten-64-to-32",
    "-DCOMPONENT_BUILD",
    "-D_LIBCPP_ABI_UNSTABLE",
    "-D_LIBCPP_ENABLE_NODISCARD",
    "-D_LIBCPP_HAS_NO_VENDOR_AVAILABILITY_ANNOTATIONS",
#    "-D_DEBUG",  # DEBUG / non-DEBUG flag has to be consitent with the rest of the project
    "-DDYNAMIC_ANNOTATIONS_ENABLED=1",
    "-DUSE_EGL",
    "-D_WTL_NO_AUTOMATIC_NAMESPACE",
    "-DGOOGLE_PROTOBUF_NO_RTTI",
    "-DGOOGLE_PROTOBUF_NO_STATIC_INITIALIZER",
    "-DPROTOBUF_USE_DLLS",
    "-DABSL_CONSUME_DLL",
    "-DBORINGSSL_SHARED_LIBRARY",
#    "-D__DATE__=",  # results in build errors, hence removed
#    "-D__TIME__=",  # results in build errors, hence removed
#    "-D__TIMESTAMP__=",  # results in build errors, hence removed
    "-DPROTOBUF_ALLOW_DEPRECATED=1",
]

DEFAULT_MINI_CHROMIUM_WIN_COPTS = DEFAULT_MINI_CHROMIUM_BASE_COPTS + [
    "-Wno-builtin-macro-redefined",
    "-Wno-nonportable-include-path",
    "-Wno-undefined-bool-conversion",
    "-Wno-tautological-undefined-compare",
    "-Wno-trigraphs",
    "-Wno-deprecated-declarations",
    "-DUSE_AURA=1",
    "-DCR_CLANG_REVISION=\"llvmorg-13-init-7296-ga749bd76-2\"",
    "-D_HAS_NODISCARD",
    "-D_LIBCPP_NO_AUTO_LINK",
    "-D_LIBCPP_HIDE_FROM_ABI=_LIBCPP_HIDDEN",
    "-D__STD_C",
    "-D_CRT_RAND_S",
    "-D_CRT_SECURE_NO_DEPRECATE",
    "-D_SCL_SECURE_NO_DEPRECATE",
    "-D_ATL_NO_OPENGL",
    "-D_WINDOWS",
    "-DCERT_CHAIN_PARA_HAS_EXTRA_FIELDS",
    "-DPSAPI_VERSION=2",
    "-DWIN32",
    "-D_SECURE_ATL",
    "-DWINAPI_FAMILY=WINAPI_FAMILY_DESKTOP_APP",
    "-DWIN32_LEAN_AND_MEAN",
    "-DNOMINMAX",
    "-D_UNICODE",
    "-DUNICODE",
    "-DNTDDI_VERSION=NTDDI_WIN10_VB",
    "-D_WIN32_WINNT=0x0A00",
    "-DWINVER=0x0A00",
    "-DWEBP_EXTERN=extern",
    "-DVK_USE_PLATFORM_WIN32_KHR",
]

DEFAULT_MINI_CHROMIUM_LINUX_COPTS = DEFAULT_MINI_CHROMIUM_BASE_COPTS + [
    "-Wno-strict-aliasing",
    "-fno-exceptions",
    "-Wall",
    "-Werror",
#    "-Wextra",  # results in build errors, hence removed
    "-Wno-sign-compare",
    "-Wno-error=unreachable-code",
    "-Wno-unused-private-field",
    "-Wno-c++98-compat-pedantic",
    "-Wno-comment",
    "-Wno-ignored-qualifiers",
    "-Wno-unused-const-variable",
    "-Wno-unknown-warning-option",
    "-DUSE_UDEV -DUSE_AURA=1",
    "-DUSE_GLIB=1",
    "-DUSE_NSS_CERTS=1",
    "-DUSE_OZONE=1",
    "-DUSE_X11=1",
    "-D_FILE_OFFSET_BITS=64",
    "-D_LARGEFILE_SOURCE",
    "-D_LARGEFILE64_SOURCE",
    "-D_GNU_SOURCE",
    "-DCR_CLANG_REVISION=\"llvmorg-13-init-7296-ga749bd76-1\"",
    "-D__STDC_CONSTANT_MACROS",
    "-D__STDC_FORMAT_MACROS",
    "-D_LIBCPP_ABI_VERSION=Cr",
    "-D_LIBCPP_DEBUG=0",
    "-DCR_LIBCXX_REVISION=8fa87946779682841e21e2da977eccfb6cb3bded",
    "-DCR_SYSROOT_HASH=43a87bbebccad99325fdcf34166295b121ee15c7",
    "-DGLIB_VERSION_MAX_ALLOWED=GLIB_VERSION_2_40",
    "-DGLIB_VERSION_MIN_REQUIRED=GLIB_VERSION_2_40",
    "-DWEBP_EXTERN=extern",
    "-DVK_USE_PLATFORM_XCB_KHR",
    "-DGL_GLEXT_PROTOTYPES",
    "-DUSE_GLX",
    "-DHAVE_PTHREAD",
]

# Build flags used to build mini_chromium.
DEFAULT_MINI_CHROMIUM_COPTS = select({
	"@platforms//os:linux": DEFAULT_MINI_CHROMIUM_LINUX_COPTS,
	"@platforms//os:windows": DEFAULT_MINI_CHROMIUM_WIN_COPTS,
})

MINI_CHROMIUM_HDRS = [
    "base/atomicops.h",
    "base/auto_reset.h",
    "base/bit_cast.h",
    "base/check.h",
    "base/check_op.h",
    "base/compiler_specific.h",
    "base/cxx17_backports.h",
    "base/files/file_path.h",
    "base/files/scoped_file.h",
    "base/format_macros.h",
    "base/logging.h",
    "base/ignore_result.h",
    "base/memory/free_deleter.h",
    "base/memory/page_size.h",
    "base/metrics/histogram_functions.h",
    "base/metrics/histogram_macros.h",
    "base/metrics/persistent_histogram_allocator.h",
    "base/notreached.h",
    "base/numerics/checked_math.h",
    "base/numerics/clamped_math.h",
    "base/numerics/safe_conversions.h",
    "base/numerics/safe_math.h",
    "base/posix/eintr_wrapper.h",
    "base/rand_util.h",
    "base/scoped_generic.h",
    "base/strings/string_number_conversions.h",
    "base/strings/string_piece.h",
    "base/strings/string_util.h",
    "base/strings/stringprintf.h",
    "base/strings/utf_string_conversions.h",
    "base/synchronization/lock.h",
    "base/threading/thread_local_storage.h",
    "build/build_config.h",
]

MINI_CHROMIUM_SRCS = [
    "base/compiler_specific.h",
    "base/debug/alias.cc",
    "base/debug/alias.h",
    "base/files/file_path.cc",
    "base/files/file_util.h",
    "base/files/scoped_file.cc",
    "base/logging.cc",
    "base/memory/scoped_policy.h",
    "base/numerics/checked_math_impl.h",
    "base/numerics/clamped_math_impl.h",
    "base/numerics/safe_math_clang_gcc_impl.h",
    "base/numerics/safe_math_shared_impl.h",
    "base/process/memory.cc",
    "base/process/memory.h",
    "base/rand_util.cc",
    "base/scoped_clear_last_error.h",
    "base/strings/string_number_conversions.cc",
    "base/strings/stringprintf.cc",
    "base/strings/sys_string_conversions.h",
    "base/strings/utf_string_conversion_utils.cc",
    "base/strings/utf_string_conversion_utils.h",
    "base/strings/utf_string_conversions.cc",
    "base/synchronization/condition_variable.h",
    "base/synchronization/lock.cc",
    "base/synchronization/lock_impl.h",
    "base/template_util.h",
    "base/third_party/icu/icu_utf.cc",
    "base/third_party/icu/icu_utf.h",
    "base/threading/thread_local_storage.cc",
]

MINI_CHROMIUM_LINUX_HDRS = [
    "base/posix/safe_strerror.h",
]

MINI_CHROMIUM_LINUX_SRCS = [
    "base/files/file_util_posix.cc",
    "base/memory/page_size_posix.cc",
    "base/posix/safe_strerror.cc",
    "base/strings/string_util_posix.h",
    "base/synchronization/condition_variable_posix.cc",
    "base/synchronization/lock_impl_posix.cc",
    "base/threading/thread_local_storage_posix.cc",
]

MINI_CHROMIUM_WINDOWS_SRCS = [
    "base/memory/page_size_win.cc",
    "base/scoped_clear_last_error_win.cc",
    "base/strings/string_util_win.cc",
    "base/strings/string_util_win.h",
    "base/synchronization/lock_impl_win.cc",
    "base/threading/thread_local_storage_win.cc",
]

ALL_HDRS_SRCS_LINUX = MINI_CHROMIUM_SRCS + MINI_CHROMIUM_LINUX_SRCS + \
                MINI_CHROMIUM_HDRS + MINI_CHROMIUM_LINUX_HDRS

ALL_HDRS_SRCS_WINDOWS = MINI_CHROMIUM_SRCS + MINI_CHROMIUM_WINDOWS_SRCS + \
                MINI_CHROMIUM_HDRS

UNIQUE_HDRS_SRCS_LINUX = dict(zip(ALL_HDRS_SRCS_LINUX, ALL_HDRS_SRCS_LINUX)).keys()

UNIQUE_HDRS_SRCS_WINDOWS = dict(zip(ALL_HDRS_SRCS_WINDOWS, ALL_HDRS_SRCS_WINDOWS)).keys()

# This is the actual code being built using base as the first dir in the include
cc_library(
    name = "base_lib",
    srcs = select({
               "@platforms//os:linux": UNIQUE_HDRS_SRCS_LINUX,
               "@platforms//os:windows": UNIQUE_HDRS_SRCS_WINDOWS,
           }),
    copts = DEFAULT_MINI_CHROMIUM_COPTS,
    textual_hdrs = [
        "base/atomicops_internals_atomicword_compat.h",
        "base/atomicops_internals_portable.h",
        "base/numerics/safe_conversions_impl.h",
        "base/numerics/safe_math_arm_impl.h",
        "base/numerics/safe_conversions_arm_impl.h",
        "base/sys_byteorder.h",
    ],
)

# This is a trick so we can add mini_chromium/ as the first dir in the include
# path for the rest of Maldoca.
cc_library(
    name = "base",
    hdrs = MINI_CHROMIUM_HDRS +
           select({
               "@platforms//os:linux": MINI_CHROMIUM_LINUX_HDRS,
               "@platforms//os:windows": [],
           }),
    copts = DEFAULT_MINI_CHROMIUM_COPTS,
    include_prefix = "mini_chromium",
    textual_hdrs = [
        "base/atomicops_internals_atomicword_compat.h",
        "base/atomicops_internals_portable.h",
        "base/numerics/safe_conversions_impl.h",
        "base/numerics/safe_math_arm_impl.h",
        "base/numerics/safe_conversions_arm_impl.h",
        "base/sys_byteorder.h",
    ],
    deps = [":base_lib"],
)
