load(
    "@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl",
    "cc_test_with_tflite",
)

package(
    default_visibility = [
        "//visibility:private",
    ],
    licenses = ["notice"],  # Apache 2.0
)

# bazel test tensorflow_lite_support/c/test/task/audio:audio_classifier_test
cc_test_with_tflite(
    name = "audio_classifier_test",
    srcs = ["audio_classifier_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/audio:test_audio_clips",
        "//tensorflow_lite_support/cc/test/testdata/task/audio:test_models",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/c/task/audio:audio_classifier",
        "@org_tensorflow//tensorflow/lite/core/shims:cc_shims_test_util",
    ],
    deps = [
        "//tensorflow_lite_support/c:common",
        "//tensorflow_lite_support/c/task/audio/core:audio_buffer",
        "//tensorflow_lite_support/c/task/processor:classification_result",
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/audio/utils:wav_io",
        "//tensorflow_lite_support/cc/test:test_utils",
    ],
)
