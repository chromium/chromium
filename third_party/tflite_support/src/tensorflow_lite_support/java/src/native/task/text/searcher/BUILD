load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite", "jni_binary_with_tflite")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files(["text_searcher_jni.cc"])

cc_library_with_tflite(
    name = "text_searcher_native",
    tflite_jni_binaries = [
        ":libtask_text_jni.so",
    ],
)

jni_binary_with_tflite(
    name = "libtask_text_jni.so",
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    tflite_deps = [
        ":native_without_resolver",
        # Pack the universal_sentence_encoder_qa_op_resolver (built-in + USE custom ops)
        # to the Task Java Library by default.
        # Use `native_without_resolver` if a custom set of ops is preferred.
        ":universal_sentence_encoder_qa_op_register",
    ],
)

cc_library_with_tflite(
    name = "universal_sentence_encoder_qa_op_register",
    srcs = [
        "universal_sentence_encoder_qa_op_register.cc",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/text/utils:text_op_resolver",
    ],
)

# Shared native logic for TextSearcher. Combine this target and customized
# version of op_resolver to build customized text_searcher_native target.
cc_library_with_tflite(
    name = "native_without_resolver",
    srcs = [
        "text_searcher_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/core:task_jni_utils.cc",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/text:text_searcher",
        "//tensorflow_lite_support/cc/utils:jni_utils",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/processor/proto:search_result_cc_proto",
        "//tensorflow_lite_support/cc/task/text/proto:text_searcher_options_cc_proto",
        "//tensorflow_lite_support/java/jni",
    ],
)
