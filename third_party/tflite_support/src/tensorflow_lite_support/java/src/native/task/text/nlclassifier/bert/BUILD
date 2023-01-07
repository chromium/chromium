load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite", "jni_binary_with_tflite")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "bert_nl_classifier_jni.cc",
])

cc_library_with_tflite(
    name = "bert_nl_classifier_native",
    tflite_jni_binaries = [
        ":libtask_text_jni.so",
    ],
)

jni_binary_with_tflite(
    name = "libtask_text_jni.so",
    srcs = [
        "bert_nl_classifier_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/core:task_jni_utils.cc",
    ],
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/text:bert_nl_classifier",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "//tensorflow_lite_support/java/src/native/task/text/nlclassifier:nl_classifier_jni_utils",
    ],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/text/proto:bert_nl_classifier_options_proto_inc",
        "//tensorflow_lite_support/java/jni",
    ],
)
