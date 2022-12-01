# TFLite Support is a toolkit that helps users to develop ML and deploy TFLite
# models onto mobile devices.

load("//tools/build_defs/license:license.bzl", "license")

package(
    default_applicable_licenses = ["//tensorflow_lite_support:license"],
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

license(
    name = "license",
    package_name = "tensorflow_lite_support",
)

exports_files(["LICENSE"])

package_group(
    name = "users",
    includes = [
        ":internal",
    ],
    packages = [
    ],
)

package_group(
    name = "internal",
    packages = [
        "//tensorflow_lite_support/...",
    ],
)

# Config setting for determining if we are building for Android.
config_setting(
    name = "android",
    values = {"crosstool_top": "//external:android/crosstool"},
    visibility = ["//visibility:public"],
)

# Config setting for determining if we are building for macos.
config_setting(
    name = "macos",
    values = {
        "apple_platform_type": "macos",
        "cpu": "darwin",
    },
    visibility = ["//visibility:public"],
)
