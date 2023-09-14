load("@build_bazel_rules_android//android:rules.bzl", "android_library")

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],  # Apache 2.0
)

filegroup(
    name = "audio_classifier_src",
    srcs = glob(["**/*.java"]),
)

# Default target that uses BuiltInOpResolver, registers all built-in OPs.
android_library(
    name = "audio_classifier",
    exports = [
        ":audio_classifier_java",
        "//tensorflow_lite_support/java/src/native/task/audio/classifier:audio_classifier_native",
    ],
)

# Java-only target, needs to be used together with a native target similar to
# //third_party/tensorflow_lite_support/java/src/native/task/audio/classifier:audio_classifier_native.
# Use this target when you want to provide a MutableOpResolver with customized
# OPs and/or a subset of BuiltInOps to reduce binary size.
android_library(
    name = "audio_classifier_java",
    srcs = [":audio_classifier_src"],
    manifest = "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/audio:AndroidManifest.xml",
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "@com_google_auto_value",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java_stable",
    ],
)
