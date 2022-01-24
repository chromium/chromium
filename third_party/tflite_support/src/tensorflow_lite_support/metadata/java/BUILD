# Description:
# TensorFlow Lite Support API in Java for metadata.

load("@build_bazel_rules_android//android:rules.bzl", "android_library")
load("@org_tensorflow//tensorflow/java:build_defs.bzl", "JAVACOPTS")

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],  # Apache 2.0
)

METADATA_SRCS = glob(
    ["src/java/org/tensorflow/lite/support/metadata/**/*.java"],
)

android_library(
    name = "tensorflowlite_support_metadata",
    srcs = METADATA_SRCS,
    javacopts = JAVACOPTS,
    manifest = "AndroidManifest.xml",
    deps = [
        "//tensorflow_lite_support/metadata:metadata_schema_fbs_android",
        "//tensorflow_lite_support/metadata:schema_fbs_android",
        "@org_checkerframework_qual",
    ],
)

alias(
    name = "tensorflow-lite-support-metadata",
    actual = ":tensorflowlite_support_metadata",
)

java_library(
    name = "tensorflowlite_support_metadata_lib",
    srcs = METADATA_SRCS,
    javacopts = JAVACOPTS,
    resource_jars = [
        "//tensorflow_lite_support/metadata:libmetadata_schema_java.jar",
        "//tensorflow_lite_support/metadata:libschema_fbs_java.jar",
    ],
    deps = [
        "//tensorflow_lite_support/metadata:metadata_schema_java",
        "//tensorflow_lite_support/metadata:schema_fbs_java",
        "@org_checkerframework_qual",
    ],
)

alias(
    name = "tensorflow-lite-support-metadata-lib",
    actual = ":tensorflowlite_support_metadata_lib",
)
