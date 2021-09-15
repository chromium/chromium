load("@build_bazel_rules_android//android:rules.bzl", "android_library")
load("@org_tensorflow//tensorflow/java:build_defs.bzl", "JAVACOPTS")
load("@org_tensorflow//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

filegroup(
    name = "nl_classifier_src",
    srcs = glob(["**/*.java"]),
)

# Java-only target, need to be used together with a native target similar to
# third_party/tensorflow_lite_support/java/src/native/task/text/nlclassifier:nl_classifier_native.
# Use this target when you want to provide a MutableOpResolver with customized
# OPs and/or a subset of BuiltInOps to reduce binary size.
android_library(
    name = "nl_classifier_java",
    srcs = [
        "NLClassifier.java",
    ],
    javacopts = JAVACOPTS,
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "@com_google_auto_value",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java",
    ],
)

# Default target that uses BuiltInOpResolver, registers all built-in OPs.
android_library(
    name = "nl_classifier",
    srcs = [
        "NLClassifier.java",
    ],
    javacopts = JAVACOPTS,
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "//tensorflow_lite_support/java/src/native/task/text/nlclassifier:nl_classifier_native",
        "@com_google_auto_value",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java",
    ],
)

# AAR target for OSS release.
#
# bazel build -c opt --config=monolithic --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
#   tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text/nlclassifier:nl-classifier
aar_with_jni(
    name = "nl-classifier",
    android_library = ":nl_classifier",
)

# Default target that uses BuiltInOpResolver, registers all built-in OPs.
android_library(
    name = "bert_nl_classifier",
    srcs = [
        "BertNLClassifier.java",
    ],
    javacopts = JAVACOPTS,
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "//tensorflow_lite_support/java/src/native/task/text/nlclassifier/bert_nlclassifier:bert_nl_classifier_native",
    ],
)

# AAR target for OSS release.
#
# bazel build -c opt --config=monolithic --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
#   tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text/nlclassifier:bert-nl-classifier
aar_with_jni(
    name = "bert-nl-classifier",
    android_library = ":bert_nl_classifier",
)
