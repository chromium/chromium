load("@org_tensorflow//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")
load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "android_library_with_tflite")
load("@build_bazel_rules_android//android:rules.bzl", "android_library")

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],  # Apache 2.0
)

filegroup(
    name = "bert_clu_annotator_src",
    srcs = glob(["**/*.java"]),
)

# Default target that uses BuiltInOpResolver, registers all built-in OPs.
android_library_with_tflite(
    name = "bert_clu_annotator",
    tflite_exports = [
        "//tensorflow_lite_support/java/src/native/task/text/bertclu:bert_clu_annotator_native",
    ],
    exports = [
        ":bert_clu_annotator_java",
    ],
)

# Java-only target, need to be used together with a native target similar to
# //third_party/tensorflow_lite_support/java/src/native/task/text/bertclu:bert_clu_annotator_native.
# Use this target when you want to provide a MutableOpResolver with customized
# OPs and/or a subset of BuiltInOps to reduce binary size.
android_library(
    name = "bert_clu_annotator_java",
    srcs = glob(["*.java"]),
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "@com_google_auto_value",
        "@maven//:androidx_annotation_annotation",
    ],
)

# AAR target for OSS release.
#
# bazel build -c opt --config=monolithic --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
#   tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text/bertclu:bert-clu-annotator
aar_with_jni(
    name = "bert-clu-annotator",
    android_library = ":bert_clu_annotator",
)
