# Description:
# TensorFlow Lite Support API in Java.

load("@org_tensorflow//tensorflow/java:build_defs.bzl", "JAVACOPTS")
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

android_library(
    name = "tensorflowlite_support",
    srcs = glob(
        ["src/java/org/tensorflow/lite/support/**/*.java"],
    ),
    javacopts = JAVACOPTS,
    manifest = "AndroidManifest.xml",
    deps = [
        "@org_checkerframework_qual",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite",
    ],
)

android_library(
    name = "tensorflowlite_support_java",
    srcs = glob(
        ["src/java/org/tensorflow/lite/support/**/*.java"],
    ),
    javacopts = JAVACOPTS,
    manifest = "AndroidManifest.xml",
    deps = [
        "@org_checkerframework_qual",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java",
    ],
)

# TODO(b/156482505): Remove this target.
alias(
    name = "tensorflow-lite-support-nogpu",
    actual = ":tensorflow-lite-support",
)

# This alias matches the associated .aar library name output style.
alias(
    name = "tensorflow-lite-support",
    actual = ":tensorflowlite_support",
)

java_library(
    name = "tensorflowlite_support_precondition_lib",
    srcs = ["src/java/org/tensorflow/lite/support/common/SupportPreconditions.java"],
    javacopts = JAVACOPTS,
    deps = [
        "@org_checkerframework_qual",
    ],
)

android_library(
    name = "tensorflowlite_support_precondition",
    srcs = ["src/java/org/tensorflow/lite/support/common/SupportPreconditions.java"],
    javacopts = JAVACOPTS,
    manifest = "AndroidManifest.xml",
    deps = [
        "@org_checkerframework_qual",
    ],
)
