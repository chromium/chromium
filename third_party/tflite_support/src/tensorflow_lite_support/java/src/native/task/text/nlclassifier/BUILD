load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite", "jni_binary_with_tflite")

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "nl_classifier_jni.cc",
])

# Default native target for nl_classifier to provide BuiltInOpResolver.
cc_library_with_tflite(
    name = "nl_classifier_native",
    tflite_jni_binaries = [
        ":libtask_text_jni.so",
    ],
    deps = [
        "@org_tensorflow//tensorflow/lite/kernels:deprecated_backends",
    ],
)

# Note: "libtask_text_jni" is hardcoded in Java to look up the .so, therefore
# the name should remain the same when creating customized version of
# nl_classifier_native
jni_binary_with_tflite(
    name = "libtask_text_jni.so",
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    tflite_deps = [
        ":native_without_resolver",
        "//tensorflow_lite_support/java/src/native/task/core:builtin_op_resolver",
    ],
)

# Shared native logic for NLClassifier. Combine this target and customized
# version of op_resolver to build customized nl_classifier_native target.
cc_library_with_tflite(
    name = "native_without_resolver",
    srcs = [
        "nl_classifier_jni.cc",
        "//tensorflow_lite_support/java/src/native/task/core:task_jni_utils.cc",
    ],
    tflite_deps = [
        ":nl_classifier_jni_utils",
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier",
        "//tensorflow_lite_support/cc/utils:jni_utils",
    ],
    deps = [
        "//tensorflow_lite_support/cc/task/core/proto:base_options_proto_inc",
        "//tensorflow_lite_support/java/jni",
        "@org_tensorflow//tensorflow/lite:op_resolver",
        "@org_tensorflow//tensorflow/lite/kernels:kernel_util",
    ],
    alwayslink = 1,
)

cc_library_with_tflite(
    name = "nl_classifier_jni_utils",
    srcs = [
        "nl_classifier_jni_utils.cc",
    ],
    hdrs = [
        "nl_classifier_jni_utils.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier",
        "//tensorflow_lite_support/cc/utils:jni_utils",
    ],
    deps = [
        "//tensorflow_lite_support/java/jni",
    ],
)
