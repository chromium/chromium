load(
    "@org_tensorflow//tensorflow/lite/core/shims:cc_library_with_tflite.bzl",
    "cc_library_with_tflite",
)

package(
    default_visibility = ["//visibility:public"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library_with_tflite(
    name = "tflite_modules",
    srcs = ["tflite_modules.cc"],
    hdrs = ["tflite_modules.h"],
    tflite_deps = [
        "//tensorflow_lite_support/cc/task/core:tflite_engine",
    ],
    deps = [
        ":bert_utils",
        ":constants",
        ":intent_repr",
        ":slot_tagging_output",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/task/core/proto:class_cc_proto",
        "//tensorflow_lite_support/cc/task/text/proto:bert_clu_annotator_options_proto_inc",
        "//tensorflow_lite_support/cc/task/text/proto:clu_proto_inc",
        "//tensorflow_lite_support/cc/text/tokenizers:bert_tokenizer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite:framework_stable",
        "@org_tensorflow//tensorflow/lite:string_util",
        "@org_tensorflow//tensorflow/lite/kernels:kernel_util",
    ],
)

cc_library(
    name = "tflite_test_utils",
    testonly = True,
    srcs = ["tflite_test_utils.cc"],
    hdrs = ["tflite_test_utils.h"],
    deps = [
        "@org_tensorflow//tensorflow/lite:string_util",
        "@org_tensorflow//tensorflow/lite:type_to_tflitetype",
        "@org_tensorflow//tensorflow/lite/c:common",
    ],
)

cc_library(
    name = "bert_utils",
    srcs = ["bert_utils.cc"],
    hdrs = ["bert_utils.h"],
    deps = [
        ":constants",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/text/tokenizers:bert_tokenizer",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "constants",
    srcs = ["constants.cc"],
    hdrs = ["constants.h"],
)

cc_library(
    name = "intent_repr",
    srcs = ["intent_repr.cc"],
    hdrs = ["intent_repr.h"],
    deps = [
        ":constants",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "slot_repr",
    srcs = [
        "slot_repr.cc",
    ],
    hdrs = [
        "slot_repr.h",
    ],
    deps = [
        ":constants",
        "//tensorflow_lite_support/cc/port:status_macros",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)

cc_library(
    name = "slot_tagging_output",
    srcs = ["slot_tagging_output.cc"],
    hdrs = ["slot_tagging_output.h"],
    deps = [
        ":slot_repr",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/task/text/proto:clu_proto_inc",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/types:span",
    ],
)
