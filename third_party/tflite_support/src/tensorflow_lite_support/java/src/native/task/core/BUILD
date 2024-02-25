load(
    "@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl",
    "cc_library_with_tflite",
)

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files(["task_jni_utils.cc"])

# Default provider for BuiltInOpResover. Create your own target, overwrite the
# function to provide a MutableOpResolver for customized OPs and/or a subset of
# builtin OPs.
cc_library_with_tflite(
    name = "builtin_op_resolver",
    srcs = ["builtin_op_resolver.cc"],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
    ],
    deps = [
        "@org_tensorflow//tensorflow/lite:op_resolver",
    ],
)
