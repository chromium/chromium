load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite", "jni_binary_with_tflite")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files(["image_classifier_jni.cc"])

cc_library_with_tflite(
    name = "image_classifier_native",
    tflite_jni_binaries = [
        ":libtask_vision_jni.so",
    ],
)

jni_binary_with_tflite(
    name = "libtask_vision_jni.so",
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    tflite_deps = [
        ":native_without_resolver",
        "//tensorflow_lite_support/java/src/native/task/core:builtin_op_resolver",
    ],
)

# Shared native logic for ImageClassifier. Combine this target and customized
# version of op_resolver to build customized image_classifier_native target.
cc_library_with_tflite(
    name = "native_without_resolver",
    srcs = [
        "image_classifier_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/core:task_jni_utils.cc",
        "//tensorflow_lite_support/java/src/native/task/vision/core:base_vision_task_api_jni.cc",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/vision:image_classifier",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "//tensorflow_lite_support/java/src/native/task/vision:jni_utils",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/core:frame_buffer",
        "//tensorflow_lite_support/cc/task/vision/proto:bounding_box_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:classifications_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/proto:image_classifier_options_proto_inc",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/java/jni",
    ],
)
