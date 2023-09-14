load("@org_tensorflow//tensorflow/lite/java:aar_with_jni.bzl", "aar_with_jni")
load("@build_bazel_rules_android//android:rules.bzl", "android_library")
load("@org_tensorflow//tensorflow/lite:build_def.bzl", "tflite_jni_binary")

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "gpu_plugin",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:gpu_plugin",
    ],
    alwayslink = 1,  # For registration to always run.
)

cc_library(
    name = "nnapi_plugin",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:nnapi_plugin",
    ],
    alwayslink = 1,  # For registration to always run.
)

cc_library(
    name = "hexagon_plugin",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:hexagon_plugin",
    ],
    alwayslink = 1,  # For registration to always run.
)

cc_library(
    name = "xnnpack_plugin",
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:xnnpack_plugin",
    ],
    alwayslink = 1,  # For registration to always run.
)

# To use the edgetpu_coral_plugin externally, add the following flags to the bazel command:
# --define darwinn_portable=1
cc_library(
    name = "edgetpu_coral_plugin",
    srcs = ["edgetpu_coral_plugin.cc"],
    visibility = [
        "//visibility:public",
    ],
    deps = [
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@com_google_glog//:glog",
        "@libedgetpu//tflite/public:edgetpu_c",
        "@libedgetpu//tflite/public:oss_edgetpu_direct_all",  # buildcleaner: keep
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_fbs",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:delegate_registry",
    ],
    alwayslink = 1,  # For registration to always run.
)

# To test it externally, plugin a Coral device, and run the following command:
# bazel test tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin_test \
# --define darwinn_portable=1
cc_test(
    name = "edgetpu_coral_plugin_test",
    srcs = ["edgetpu_coral_plugin_test.cc"],
    data = [
        "//tensorflow_lite_support/acceleration/configuration/testdata:test_files",
    ],
    tags = [
        "manual",
        "notap",  # Requires edge TPU device.
    ],
    deps = [
        ":edgetpu_coral_plugin",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "@com_google_googletest//:gtest_main",
        "@org_tensorflow//tensorflow/lite:framework",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:configuration_fbs",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:delegate_registry",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
    ],
)

# Targets for delegate plugin library Maven release.

# GPU delegate plugin library.
tflite_jni_binary(
    name = "libgpu_delegate_plugin.so",
    linkscript = "//tensorflow_lite_support/java:default_version_script.lds",
    visibility = ["//visibility:private"],
    deps = [
        "@org_tensorflow//tensorflow/lite/acceleration/configuration/c:gpu_plugin",
    ],
)

cc_library(
    name = "gpu_delegate_plugin_native",
    srcs = [
        ":libgpu_delegate_plugin.so",
    ],
    visibility = ["//visibility:private"],
)

# Android target of Acceleration@Scale GPU plugin.
# Use this target when GPU delegate is selected in the Task Library Java API.
android_library(
    name = "gpu_delegate_plugin_android",
    visibility = [
        "//visibility:public",
    ],
    exports = [":gpu_delegate_plugin_native"],
)

# AAR target of Acceleration@Scale GPU acceleration for OSS release.
aar_with_jni(
    name = "gpu-delegate-plugin",
    android_library = ":gpu_delegate_plugin_android",
)
