load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "android_library_with_tflite")
load("@org_tensorflow//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "AndroidManifest.xml",
])

android_library_with_tflite(
    name = "task_library_vision",
    srcs = [
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision/classifier:image_classifier_src",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision/core:base_vision_api_src",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision/detector:object_detector_src",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision/searcher:image_searcher_src",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision/segmenter:image_segmenter_src",
    ],
    manifest = "AndroidManifest.xml",
    tflite_exports = [
        "//tensorflow_lite_support/java/src/native/task/vision:task_vision_native",
    ],
    visibility = ["//visibility:public"],
    # LINT.IfChange(dep)
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "@com_google_auto_value",
        "@maven//:androidx_annotation_annotation",
        "@maven//:com_google_android_odml_image",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java_stable",
    ],
    # LINT.ThenChange(<INTERNAL>/release/build_task_pom.sh:dep)
)

# AAR target for OSS release.
#
# bazel build -c opt --config=monolithic --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
#   tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/vision:task-library-vision
aar_with_jni(
    name = "task-library-vision",
    android_library = ":task_library_vision",
)
