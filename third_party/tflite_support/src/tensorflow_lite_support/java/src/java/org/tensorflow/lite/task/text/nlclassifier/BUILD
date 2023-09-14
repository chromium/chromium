load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "android_library_with_tflite")
load("@org_tensorflow//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")
load("@build_bazel_rules_android//android:rules.bzl", "android_library")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

filegroup(
    name = "nl_classifier_src",
    srcs = glob(["**/*.java"]),
)

# Default target that uses BuiltInOpResolver, registers all built-in OPs.
android_library_with_tflite(
    name = "nl_classifier",
    tflite_exports = [
        "//tensorflow_lite_support/java/src/native/task/text/nlclassifier:nl_classifier_native",
    ],
    exports = [
        ":nl_classifier_java",
    ],
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
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "@com_google_auto_value",
        "@maven//:androidx_annotation_annotation",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java_stable",
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
android_library_with_tflite(
    name = "bert_nl_classifier",
    srcs = [
        "BertNLClassifier.java",
    ],
    tflite_exports = [
        "//tensorflow_lite_support/java/src/native/task/text/nlclassifier/bert:bert_nl_classifier_native",
    ],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "@com_google_auto_value",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java",
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
