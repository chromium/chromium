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

cc_test_with_tflite(
    name = "image_preprocessor_test",
    srcs = ["image_preprocessor_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_images",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/processor:image_preprocessor",
        "@org_tensorflow//tensorflow/lite:test_util",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/vision/utils:frame_buffer_common_utils",
        "//tensorflow_lite_support/cc/task/vision/utils:image_utils",
        "//tensorflow_lite_support/cc/test:test_utils",
        "@com_google_absl//absl/status",
    ],
)

cc_test_with_tflite(
    name = "embedding_searcher_test",
    srcs = ["embedding_searcher_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/vision:protos",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_indices",
        "//tensorflow_lite_support/cc/test/testdata/task/vision:test_models",
    ],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite:test_util",
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:external_file_handler",
        "//tensorflow_lite_support/cc/task/core/proto:external_file_proto_inc",
        "//tensorflow_lite_support/cc/task/processor:embedding_searcher",
        "//tensorflow_lite_support/cc/task/processor/proto:search_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:search_result_cc_proto",
        "//tensorflow_lite_support/cc/test:test_utils",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)
