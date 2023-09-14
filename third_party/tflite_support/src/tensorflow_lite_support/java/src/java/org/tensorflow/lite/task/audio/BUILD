load("@org_tensorflow//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")
load("@build_bazel_rules_android//android:rules.bzl", "android_library")

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "AndroidManifest.xml",
])

android_library(
    name = "task_library_audio",
    srcs = [
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/audio/classifier:audio_classifier_src",
    ],
    # TODO(b/163039980): Use JAVACOPTS in TF. "-Xep:RemoveUnusedImports:ERROR" wierdly break the build.
    manifest = "AndroidManifest.xml",
    visibility = ["//visibility:public"],
    # LINT.IfChange(dep)
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "//tensorflow_lite_support/java/src/native/task/audio:task_audio_native",
        "@com_google_auto_value",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java_stable",
    ],
    # LINT.ThenChange(<INTERNAL>/release/build_task_pom.sh:dep)
)

# AAR target for OSS release.
#
# bazel build -c opt --config=monolithic --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
#   tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/audio:task-library-audio
aar_with_jni(
    name = "task-library-audio",
    android_library = ":task_library_audio",
)
