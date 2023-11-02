load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite", "jni_binary_with_tflite")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library_with_tflite(
    name = "task_audio_native",
    srcs = [
        ":libtask_audio_jni.so",
    ],
)

jni_binary_with_tflite(
    name = "libtask_audio_jni.so",
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    tflite_deps = [
        ":task_audio_jni_lib",
    ],
)

cc_library_with_tflite(
    name = "task_audio_jni_lib",
    srcs = [
        "//tensorflow_lite_support/java/src/native/task/audio/classifier:audio_classifier_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/core:task_jni_utils.cc",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/java/src/native/task/core:builtin_op_resolver",
        "//tensorflow_lite_support/cc/task/audio:audio_classifier",
        "//tensorflow_lite_support/cc/utils:jni_utils",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/audio/core:audio_buffer",
        "//tensorflow_lite_support/cc/task/audio/proto:audio_classifier_options_cc_proto",
        "//tensorflow_lite_support/cc/task/audio/proto:class_proto_inc",
        "//tensorflow_lite_support/cc/task/audio/proto:classifications_proto_inc",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/java/jni",
    ],
)
