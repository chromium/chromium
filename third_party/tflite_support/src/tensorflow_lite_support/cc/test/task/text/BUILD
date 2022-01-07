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
    name = "bert_nl_classifier_test",
    srcs = ["bert_nl_classifier_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:bert_nl_classifier_models",
    ],
    tflite_deps = [
        "@org_tensorflow//tensorflow/lite/core/shims:cc_shims_test_util",
        "//tensorflow_lite_support/cc/task/text:bert_nl_classifier",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/test:test_utils",
    ],
)

cc_test(
    name = "bert_question_answerer_test",
    timeout = "long",
    srcs = ["bert_question_answerer_test.cc"],
    data = [
        "//tensorflow_lite_support/cc/test/testdata/task/text:albert_model",
        "//tensorflow_lite_support/cc/test/testdata/task/text:mobile_bert_model",
    ],
    tags = [
        "optonly",  # The test takes long, and only run with -c opt.
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/task/text:bert_question_answerer",
        "//tensorflow_lite_support/cc/test:test_utils",
        "@org_tensorflow//tensorflow/lite/core/shims:cc_shims_test_util",
    ],
)
