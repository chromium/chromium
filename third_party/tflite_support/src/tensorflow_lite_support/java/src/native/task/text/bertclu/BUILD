load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite", "jni_binary_with_tflite")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files(["bert_clu_annotator_jni.cc"])

jni_binary_with_tflite(
    name = "libtask_text_jni.so",
    srcs = [
        "bert_clu_annotator_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/core:task_jni_utils.cc",
    ],
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/text:bert_clu_annotator",
        "//tensorflow_lite_support/cc/task/text:clu_annotator",
        "//tensorflow_lite_support/cc/utils:jni_utils",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/core/proto:class_cc_proto",
        "//tensorflow_lite_support/cc/task/text/proto:bert_clu_annotator_options_cc_proto",
        "//tensorflow_lite_support/cc/task/text/proto:clu_cc_proto",
        "//tensorflow_lite_support/java/jni",
    ],
)

cc_library_with_tflite(
    name = "bert_clu_annotator_native",
    tflite_jni_binaries = [
        ":libtask_text_jni.so",
    ],
)
