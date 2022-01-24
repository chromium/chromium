load("@org_tensorflow//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")
load("@build_bazel_rules_android//android:rules.bzl", "android_library")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "AndroidManifest.xml",
])

filegroup(
    name = "image_segmenter_src",
    srcs = glob(["**/*.java"]),
)

android_library(
    name = "image_segmenter",
    srcs = glob(["*.java"]),
    # TODO(b/163039980): Use JAVACOPTS in TF. "-Xep:RemoveUnusedImports:ERROR" wierdly break the build.
    javacopts = ["-source 7 -target 7"],
    manifest = "AndroidManifest.xml",
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "//tensorflow_lite_support/java/src/native/task/vision/segmenter:image_segmenter_native",
        "@com_google_auto_value",
        "@maven//:androidx_annotation_annotation",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java",
    ],
)

# AAR target for OSS release.
#
# bazel build -c opt --config=monolithic --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
#   tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision/segmenter:image-segmenter
aar_with_jni(
    name = "image-segmenter",
    android_library = ":image_segmenter",
)
