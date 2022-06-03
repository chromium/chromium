package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "jni_utils",
    srcs = [
        "jni_utils.cc",
    ],
    hdrs = [
        "jni_utils.h",
    ],
    deps = [
        "@com_google_absl//absl/strings",
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
    deps = [
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
    ],
)
