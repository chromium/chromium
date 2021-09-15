# TFLite Support is a toolkit that helps users to develop ML and deploy TFLite
# models onto mobile devices.

package(
    default_visibility = ["//visibility:private"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files(["LICENSE"])

# LINT.IfChange
package_group(
    name = "users",
    packages = [
        "//tensorflow_lite_support/...",
        "//third_party/py/tensorflow_examples/...",
        "//third_party/tensorflow_models/...",
    ],
)
# Remove internal path from tensorflow_lite_support:users in the copybara file.
# LINT.ThenChange(//tensorflow_lite_support/copy.bara.sky)

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
