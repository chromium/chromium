load("@build_bazel_rules_android//android:rules.bzl", "android_library")
load("@org_tensorflow//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")
load("@org_tensorflow//tensorflow/java:build_defs.bzl", "JAVACOPTS")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files(["AndroidManifest.xml"])

android_library(
    name = "task_library_text",
    srcs = [
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text/nlclassifier:nl_classifier_src",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text/qa:bert_question_answerer_src",
    ],
    javacopts = JAVACOPTS,
    manifest = "AndroidManifest.xml",
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "//tensorflow_lite_support/java/src/native/task/text:task_text_native",
        "@com_google_auto_value",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java",
    ],
)

# AAR target for OSS release.
#
# bazel build -c opt --config=monolithic --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
#   tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text:task-library-text
aar_with_jni(
    name = "task-library-text",
    android_library = ":task_library_text",
)
