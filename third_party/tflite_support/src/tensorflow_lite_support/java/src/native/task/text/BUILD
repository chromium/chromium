load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite", "jni_binary_with_tflite")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library_with_tflite(
    name = "task_text_native",
    tflite_jni_binaries = [
        ":libtask_text_jni.so",
    ],
)

jni_binary_with_tflite(
    name = "libtask_text_jni.so",
    srcs = [
        "//tensorflow_lite_support/java/src/native/task/core:task_jni_utils.cc",
        "//tensorflow_lite_support/java/src/native/task/text/bertclu:bert_clu_annotator_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/text/nlclassifier:nl_classifier_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/text/nlclassifier/bert:bert_nl_classifier_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/text/qa:bert_question_answerer_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/text/searcher:text_searcher_jni.cc",
    ],
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier",
        "//tensorflow_lite_support/cc/task/text:bert_clu_annotator",
        "//tensorflow_lite_support/cc/task/text:clu_annotator",
        "//tensorflow_lite_support/cc/task/text:bert_nl_classifier",
        "//tensorflow_lite_support/cc/task/text:bert_question_answerer",
        "//tensorflow_lite_support/cc/task/text:text_searcher",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "//tensorflow_lite_support/java/src/native/task/text/nlclassifier:nl_classifier_jni_utils",
        # Pack the universal_sentence_encoder_qa_op_resolver (built-in + USE custom ops)
        # to the Task Text Java Library by default.
        "//tensorflow_lite_support/java/src/native/task/text/searcher:universal_sentence_encoder_qa_op_register",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/core/proto:class_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_result_cc_proto",
        "//tensorflow_lite_support/cc/task/text/proto:bert_clu_annotator_options_cc_proto",
        "//tensorflow_lite_support/cc/task/text/proto:bert_nl_classifier_options_proto_inc",
        "//tensorflow_lite_support/cc/task/text/proto:clu_cc_proto",
        "//tensorflow_lite_support/cc/task/text/proto:text_searcher_options_cc_proto",
        "//tensorflow_lite_support/java/jni",
        "@org_tensorflow//tensorflow/lite:op_resolver",
        "@org_tensorflow//tensorflow/lite/kernels:kernel_util",
    ],
)
