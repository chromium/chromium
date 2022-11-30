load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "android_library_with_tflite")
load("@org_tensorflow//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files(["AndroidManifest.xml"])

android_library_with_tflite(
    name = "task_library_text",
    srcs = [
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text/bertclu:bert_clu_annotator_src",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text/nlclassifier:nl_classifier_src",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text/qa:bert_question_answerer_src",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text/searcher:text_searcher_src",
    ],
    manifest = "AndroidManifest.xml",
    tflite_exports = [
        "//tensorflow_lite_support/java/src/native/task/text:task_text_native",
    ],
    visibility = ["//visibility:public"],
    # LINT.IfChange(dep)
    deps = [
        "//tensorflow_lite_support/java:tensorflowlite_support_java",
        "//tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/core:base_task_api",
        "@com_google_auto_value",
        "@maven//:androidx_annotation_annotation",
        "@org_tensorflow//tensorflow/lite/java:tensorflowlite_java_stable",
    ],
    # LINT.ThenChange(<INTERNAL>/release/build_task_pom.sh:dep)
)

# AAR target for OSS release.
#
# bazel build -c opt --config=monolithic --config=android_arm64 --fat_apk_cpu=x86,x86_64,arm64-v8a,armeabi-v7a \
#   tensorflow_lite_support/java/src/java/org/tensorflow/lite/task/text:task-library-text
aar_with_jni(
    name = "task-library-text",
    android_library = ":task_library_text",
    headers = [
        "//tensorflow_lite_support/c/task/text:bert_nl_classifier.h",
        "//tensorflow_lite_support/c/task/text:nl_classifier.h",
        "//tensorflow_lite_support/c/task/text:nl_classifier_common.h",
        "//tensorflow_lite_support/c/task/text:bert_question_answerer.h",
    ],
)
