# TFLite Support is a toolkit that helps users to develop ML and deploy TFLite
# models onto mobile devices.

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
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
    constraint_values = [
        "@platforms//os:android",
    ],
    visibility = ["//visibility:public"],
)

# Config setting for determining if we are building for macos.
config_setting(
    name = "macos",
    constraint_values = [
        "@platforms//os:macos",
    ],
    visibility = ["//visibility:public"],
)
