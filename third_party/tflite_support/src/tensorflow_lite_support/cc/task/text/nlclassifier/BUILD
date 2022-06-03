package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

exports_files([
    "bert_nl_classifier_c_api.h",
    "nl_classifier_c_api.h",
    "nl_classifier_c_api_common.h",
])

cc_library(
    name = "nl_classifier",
    srcs = [
        "nl_classifier.cc",
    ],
    hdrs = [
        "nl_classifier.h",
    ],
    deps = [
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:base_task_api",
        "//tensorflow_lite_support/cc/task/core:category",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/text/tokenizers:regex_tokenizer",
        "//tensorflow_lite_support/cc/text/tokenizers:tokenizer",
        "//tensorflow_lite_support/cc/utils:common_utils",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/algorithm:container",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@flatbuffers",
        "@org_tensorflow//tensorflow/lite:string",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
        "@org_tensorflow//tensorflow/lite/kernels/internal:tensor",
    ],
)

cc_library(
    name = "nl_classifier_c_api",
    srcs = [
        "nl_classifier_c_api.cc",
    ],
    hdrs = [
        "nl_classifier_c_api.h",
        "nl_classifier_c_api_common.h",
    ],
    visibility = ["//tensorflow_lite_support:__subpackages__"],
    deps = [
        ":nl_classifier",
        ":nl_classifier_c_api_common",
        "//tensorflow_lite_support/cc/task/core:category",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "bert_nl_classifier",
    srcs = [
        "bert_nl_classifier.cc",
    ],
    hdrs = [
        "bert_nl_classifier.h",
    ],
    deps = [
        ":nl_classifier",
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/cc/task/core:category",
        "//tensorflow_lite_support/cc/task/core:task_api_factory",
        "//tensorflow_lite_support/cc/task/core:task_utils",
        "//tensorflow_lite_support/cc/text/tokenizers:tokenizer",
        "//tensorflow_lite_support/cc/text/tokenizers:tokenizer_utils",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:str_format",
        "@org_tensorflow//tensorflow/lite:string",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@org_tensorflow//tensorflow/lite/core/api",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
    ],
)

cc_library(
    name = "bert_nl_classifier_c_api",
    srcs = [
        "bert_nl_classifier_c_api.cc",
    ],
    hdrs = [
        "bert_nl_classifier_c_api.h",
        "nl_classifier_c_api_common.h",
    ],
    visibility = ["//tensorflow_lite_support:__subpackages__"],
    deps = [
        ":bert_nl_classifier",
        ":nl_classifier_c_api_common",
        "//tensorflow_lite_support/cc/task/core:category",
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "nl_classifier_c_api_common",
    srcs = [
        "nl_classifier_c_api_common.cc",
    ],
    hdrs = [
        "nl_classifier_c_api_common.h",
    ],
    visibility = ["//tensorflow_lite_support:__subpackages__"],
)
