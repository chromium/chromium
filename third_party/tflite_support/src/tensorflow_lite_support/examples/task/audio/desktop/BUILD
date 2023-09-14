package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "audio_classifier_lib",
    srcs = ["audio_classifier_lib.cc"],
    hdrs = ["audio_classifier_lib.h"],
    deps = [
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/audio:audio_classifier",
        "//tensorflow_lite_support/cc/task/audio/core:audio_buffer",
        "//tensorflow_lite_support/cc/task/audio/proto:classifications_proto_inc",
        "//tensorflow_lite_support/cc/task/audio/utils:wav_io",
        "//tensorflow_lite_support/cc/task/core:category",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
    ] + select({
        "//tensorflow_lite_support/examples/task:darwinn_portable": [
            "//tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin",
        ],
        "//conditions:default": [
        ],
    }),
)

# Example usage:
# bazel run -c opt \
#  tensorflow_lite_support/examples/task/audio/desktop:audio_classifier_demo \
#  -- \
#  --model_path=/path/to/model.tflite \
#  --audio_wav_path=/path/to/wav.tflite
cc_binary(
    name = "audio_classifier_demo",
    srcs = ["audio_classifier_demo.cc"],
    deps = [
        ":audio_classifier_lib",
        "//tensorflow_lite_support/cc/task/audio:audio_classifier",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/flags:parse",
    ],
)
