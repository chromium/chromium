load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite")

package(
    licenses = ["notice"],  # Apache 2.0
)

cc_library_with_tflite(
    name = "jni_utils",
    srcs = [
        "jni_utils.cc",
    ],
    hdrs = [
        "jni_utils.h",
    ],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/acceleration/configuration/c:delegate_plugin",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:delegate_registry",
        "@org_tensorflow//tensorflow/lite/acceleration/configuration:delegate_plugin_converter",
    ],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:configuration_proto_inc",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@org_tensorflow//tensorflow/lite/java/jni",
    ],
)

cc_library(
    name = "common_utils",
    srcs = [
        "common_utils.cc",
    ],
    hdrs = [
        "common_utils.h",
    ],
    visibility = [
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
    ],
)
