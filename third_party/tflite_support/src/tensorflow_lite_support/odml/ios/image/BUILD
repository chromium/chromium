# ODML image library for iOS

load(
    "@org_tensorflow//tensorflow/lite:special_rules.bzl",
    "tflite_ios_lab_runner",
)
load(
    "@org_tensorflow//tensorflow/lite/ios:ios.bzl",
    "TFL_MINIMUM_OS_VERSION",
)
load(
    "@build_bazel_rules_apple//apple:ios.bzl",
    "ios_static_framework",
    "ios_unit_test",
)

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

SOURCES = glob([
    "sources/*.h",
    "sources/*.m",
    "sources/*.mm",
])

API_HEADERS = glob([
    "apis/*.h",
])

# Compiler flags for building regular non-test libraries.
RELEASE_COPTS = [
    # Enables language-specific warnings for Objective-C, Objective-C++, C, and C++.
    "-Wall",
    # Warns if functions, variables, and types marked with the deprecated attribute are being used.
    "-Wdeprecated-declarations",
    # Warns for errors in documentation.
    "-Wdocumentation",
    # Turns all warnings into errors.
    "-Werror",
    # Enables extra warning flags that are not enabled by -Wall.
    "-Wextra",
    # Warns if a global function is defined without a previous prototype declaration.
    "-Wmissing-prototypes",
    # From -Wextra. Disables warning when signed value is converted to unsigned value during comparison.
    "-Wno-sign-compare",
    # From -Wextra. Disables warning for unused parameters, which are common in delegate methods and block callbacks.
    "-Wno-unused-parameter",
    # Warns if a global or local variable or type declaration shadows another variable, parameter, type, class member, or instance variable.
    "-Wshadow",
    # Warns if a function is declared or defined without specifying the argument types. For a block with no args, use (void) instead of ().
    "-Wstrict-prototypes",
    # Warns if an @selector() expression is encountered with a method name that hasn't been defined yet.
    "-Wundeclared-selector",
    # Turn off warnings for headers not part of TensorFlow Lite Objective-C API.
    "--system-header-prefix=third_party/tensorflow/lite/c/",
]

# Compiler flags for building test libraries.
TEST_COPTS = RELEASE_COPTS + [
    # From -Wall. Disables warning when passing nil to a callee that requires a non-null argument.
    "-Wno-nonnull",
    # Disables warning when a global or local variable or type declaration shadows another.
    "-Wno-shadow",
]

exports_files(["apis/GMLImage.h"])

objc_library(
    name = "MLImage",
    srcs = SOURCES,
    hdrs = API_HEADERS,
    copts = RELEASE_COPTS,
    module_name = "MLImage",
    visibility = ["//visibility:public"],
    alwayslink = 1,
)

ios_static_framework(
    name = "MLImage_framework",
    hdrs = API_HEADERS,
    bundle_name = "MLImage",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    visibility = ["//visibility:public"],
    deps = [":MLImage"],
)

ios_unit_test(
    name = "tests",
    size = "small",
    minimum_os_version = TFL_MINIMUM_OS_VERSION,
    runner = tflite_ios_lab_runner("IOS_LATEST"),
    deps = [
        ":TestsLibrary",
    ],
)

objc_library(
    name = "TestsLibrary",
    testonly = 1,
    srcs = glob([
        "tests/*.m",
    ]),
    hdrs = glob([
        "apis/*.h",
        "sources/*.h",
        "tests/*.h",
    ]),
    copts = TEST_COPTS,
    data = glob([
        "resources/*.jpg",
    ]),
    deps = [
        ":MLImage",
    ],
)
