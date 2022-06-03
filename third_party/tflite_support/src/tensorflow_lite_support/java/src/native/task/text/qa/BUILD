load("@org_tensorflow//tensorflow/lite:build_def.bzl", "tflite_jni_binary")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "bert_question_answerer_jni.cc",
])

tflite_jni_binary(
    name = "libtask_text_jni.so",
    srcs = [
        "bert_question_answerer_jni.cc",
    ],
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    deps = [
        "//tensorflow_lite_support/cc/task/text/qa:bert_question_answerer",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "//tensorflow_lite_support/java/jni",
    ],
)

cc_library(
    name = "bert_question_answerer_native",
    srcs = [
        ":libtask_text_jni.so",
    ],
)
