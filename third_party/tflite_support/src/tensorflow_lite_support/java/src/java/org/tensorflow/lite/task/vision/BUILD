load("@build_bazel_rules_android//android:rules.bzl", "android_library")
load("@org_tensorflow//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "AndroidManifest.xml",
])

android_library(
    name = "task_library_vision",
    srcs = [
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision/classifier:image_classifier_src",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision/detector:object_detector_src",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision/segmenter:image_segmenter_src",
    ],
    # TODO(b/163039980): Use JAVACOPTS in TF. "-Xep:RemoveUnusedImports:ERROR" wierdly break the build.
    javacopts = ["-source 7 -target 7"],
    manifest = "AndroidManifest.xml",
    visibility = ["//visibility:public"],
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "//tensorflow_lite_support/java/src/native/task/vision:task_vision_native",
        "@com_google_auto_value",
        "@maven//:androidx_annotation_annotation",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java",
    ],
)

# AAR target for OSS release.
#
# bazel build -c opt --config=monolithic --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
#   tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision:task-library-vision
aar_with_jni(
    name = "task-library-vision",
    android_library = ":task_library_vision",
)
