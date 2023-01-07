# Description:
# TensorFlow Lite Support API in Java for metadata.

load("@build_bazel_rules_android//android:rules.bzl", "android_library")

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
    javacopts = ["--release 7"],
    deps = [
        "//tensorflow_lite_support/metadata:metadata_schema_java",
        "//tensorflow_lite_support/metadata:schema_fbs_java",
        "@org_checkerframework_qual",
    ],
)

# The target for OSS release, which includes the metadata Java library, the
# metadata schema Java binding, and the TFLite schema Java binding.
java_library(
    name = "tensorflow-lite-support-metadata-lib",
    srcs = METADATA_SRCS + [
        "//tensorflow_lite_support/metadata:metadata_schema_java_srcjar",
        "//tensorflow_lite_support/metadata:schema_fbs_java_srcjar",
    ],
    javacopts = ["--release 7"],
    # LINT.IfChange(dep)
    deps = [
        # Need to be consistent as the deps used in flatbuffer_java_library.
        "@flatbuffers//:runtime_java",
        "@org_checkerframework_qual",
    ],
    # LINT.ThenChange(<INTERNAL>/release/build_metadata_pom.sh:dep)
)
