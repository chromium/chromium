package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],  # Apache 2.0
)

cc_test(
    name = "tflite_modules_test",
    srcs = ["tflite_modules_test.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/port/default:status_matchers",
        "//tensorflow_lite_support/cc/task/text/clu_lib:tflite_modules",
        "//tensorflow_lite_support/cc/task/text/clu_lib:tflite_test_utils",
        "//tensorflow_lite_support/cc/task/text/proto:bert_clu_annotator_options_proto_inc",
        "//tensorflow_lite_support/cc/test:test_utils",
        "//tensorflow_lite_support/cc/text/tokenizers:bert_tokenizer",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite:framework",
        "@org_tensorflow//tensorflow/lite:string_util",
    ],
)

cc_test(
    name = "bert_utils_test",
    srcs = ["bert_utils_test.cc"],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/port/default:status_matchers",
        "//tensorflow_lite_support/cc/task/text/clu_lib:bert_utils",
        "//tensorflow_lite_support/cc/test:test_utils",
        "//tensorflow_lite_support/cc/text/tokenizers:bert_tokenizer",
        "@org_tensorflow//tensorflow/lite:framework",  # build_cleaner: keep (Otherwise, it fails to build in OSS)
    ],
)

cc_test(
    name = "intent_repr_test",
    srcs = [
        "intent_repr_test.cc",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/port/default:status_matchers",
        "//tensorflow_lite_support/cc/task/text/clu_lib:intent_repr",
        "//tensorflow_lite_support/cc/test:test_utils",
    ],
)

cc_test(
    name = "slot_repr_test",
    srcs = [
        "slot_repr_test.cc",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/port/default:status_matchers",
        "//tensorflow_lite_support/cc/task/text/clu_lib:slot_repr",
        "//tensorflow_lite_support/cc/test:test_utils",
        "@com_google_absl//absl/strings",
    ],
)

cc_test(
    name = "slot_tagging_output_test",
    srcs = [
        "slot_tagging_output_test.cc",
    ],
    deps = [
        "//tensorflow_lite_support/cc/port:gtest_main",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port/default:status_matchers",
        "//tensorflow_lite_support/cc/task/text/clu_lib:slot_tagging_output",
        "//tensorflow_lite_support/cc/task/text/proto:clu_proto_inc",
        "//tensorflow_lite_support/cc/test:test_utils",
    ],
)
