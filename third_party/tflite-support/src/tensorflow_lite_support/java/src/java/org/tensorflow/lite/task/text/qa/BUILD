load("@org_tensorflow//tensorflow/java:build_defs.bzl", "JAVACOPTS")
load("@build_bazel_rules_android//android:rules.bzl", "android_library")
load("@org_tensorflow//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

filegroup(
    name = "bert_question_answerer_src",
    srcs = glob(["**/*.java"]),
)

android_library(
    name = "bert_question_answerer",
    srcs = glob(["*.java"]),
    javacopts = JAVACOPTS,
    deps = [
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "//tensorflow_lite_support/java/src/native/task/text/qa:bert_question_answerer_native",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java",
    ],
)

# AAR target for OSS release.
#
# bazel build -c opt --config=monolithic --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
#   tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text/qa:bert-question-answerer
aar_with_jni(
    name = "bert-question-answerer",
    android_library = ":bert_question_answerer",
)
