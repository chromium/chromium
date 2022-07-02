load("@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl", "cc_library_with_tflite")

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library_with_tflite(
    name = "audio_classifier",
    srcs = [
        "audio_classifier.cc",
    ],
    hdrs = [
        "audio_classifier.h",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/audio:audio_classifier",
    ],
    deps = [
        "//tensorflow_lite_support/c:common",
        "//tensorflow_lite_support/c:common_utils",
        "//tensorflow_lite_support/c/task/audio/core:audio_buffer",
        "//tensorflow_lite_support/c/task/audio/utils:audio_buffer_cpp_c_utils",
        "//tensorflow_lite_support/c/task/core:base_options",
        "//tensorflow_lite_support/c/task/core/utils:base_options_utils",
        "//tensorflow_lite_support/c/task/processor:classification_options",
        "//tensorflow_lite_support/c/task/processor:classification_result",
        "//tensorflow_lite_support/c/task/processor/utils:classification_options_utils",
        "//tensorflow_lite_support/cc/task/audio/proto:audio_classifier_options_cc_proto",
        "//tensorflow_lite_support/cc/task/audio/proto:class_proto_inc",
        "//tensorflow_lite_support/cc/task/audio/proto:classifications_proto_inc",
    ],
)
