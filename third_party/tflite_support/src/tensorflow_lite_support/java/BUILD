# Description:
# TensorFlow Lite Support API in Java.

load("@build_bazel_rules_android//android:rules.bzl", "android_library")

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "AndroidManifest.xml",
    "default_version_script.lds",
    "debug_version_script.lds",
])

filegroup(
    name = "java_srcs",
    srcs = glob(["src/java/org/tensorflow/lite/support/**/*.java"]),
)

# Android Library target for TFLite Support Library. It depends on the
# TensorFlow Lite Runtime (tensorflow/lite/java:tensorflowlite),
# including its associated native code. If you don't want to introduce the native
# library into dependencies, use "tensorflowlite_support_java" instead,
# which depends on tensorflow/lite/java:tensorflowlite_api.
android_library(
    name = "tensorflowlite_support",
    srcs = [],
    manifest = "AndroidManifest.xml",
    exports = [
        ":tensorflowlite_support_java",
    ],
    deps = [
        ":tensorflowlite_support_java",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_stable",
    ],
)

# Android Library target for TFLite Support Library, excluding native code,
# and excluding dependencies on the TensorFlow Lite Runtime.
# If you use this target, a TensorFlow Lite Runtime (and it's associated
# native code) is nevertheless still required, and must be provided by a
# separate dependency.
android_library(
    name = "tensorflowlite_support_java",
    srcs = [":java_srcs"],
    manifest = "AndroidManifest.xml",
    # LINT.IfChange(dep)
    deps = [
        "@com_google_auto_value",
        "@maven//:androidx_annotation_annotation",
        "@maven//:com_google_android_odml_image",
        "@org_checkerframework_qual",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_api",
    ],
    # LINT.ThenChange(<INTERNAL>/release/build_support_api_pom.sh:dep)
)

# This alias matches the associated .aar library name output style.
alias(
    name = "tensorflow-lite-support",
    actual = ":tensorflowlite_support",
)
