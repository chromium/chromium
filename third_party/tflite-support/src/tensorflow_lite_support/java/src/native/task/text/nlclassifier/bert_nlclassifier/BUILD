load("@org_tensorflow//tensorflow/lite:build_def.bzl", "tflite_jni_binary")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "bert_nl_classifier_jni.cc",
])

cc_library(
    name = "bert_nl_classifier_native",
    srcs = [
        ":libtask_text_jni.so",
    ],
)

tflite_jni_binary(
    name = "libtask_text_jni.so",
    srcs = [
        "bert_nl_classifier_jni.cc",
    ],
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    deps = [
        "//tensorflow_lite_support/cc/task/text/nlclassifier:bert_nl_classifier",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "//tensorflow_lite_support/java/jni",
        "//tensorflow_lite_support/java/src/native/task/text/nlclassifier:nl_classifier_jni_utils",
    ],
)
