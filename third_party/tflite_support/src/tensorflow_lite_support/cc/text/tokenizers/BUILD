# This package contains C++ support libraries that Java libraries can invoke.
load("@build_bazel_rules_android//android:rules.bzl", "android_library")
load(
    "@org_tensorflow//tensorflow/lite:build_def.bzl",
    "tflite_copts",
    "tflite_jni_binary",
)

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

cc_library(
    name = "tokenizer",
    hdrs = [
        "tokenizer.h",
    ],
    deps = [
        "@com_google_absl//absl/strings",
    ],
)

cc_library(
    name = "tokenizer_jni_lib",
    srcs = [
        "tokenizer_jni_lib.cc",
    ],
    hdrs = [
        "tokenizer_jni_lib.h",
    ],
    deps = [
        ":tokenizer",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "@org_tensorflow//tensorflow/lite/java/jni",
    ],
)

cc_library(
    name = "bert_tokenizer",
    srcs = [
        "bert_tokenizer.cc",
    ],
    hdrs = [
        "bert_tokenizer.h",
    ],
    deps = [
        ":tokenizer",
        "//tensorflow_lite_support/cc/port:integral_types",
        "//tensorflow_lite_support/cc/utils:common_utils",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_googlesource_code_re2//:re2",
        "@org_tensorflow_text//tensorflow_text/core/kernels:regex_split",
        "@org_tensorflow_text//tensorflow_text/core/kernels:wordpiece_tokenizer",
    ],
)

cc_library(
    name = "bert_tokenizer_jni_lib",
    srcs = [
        "bert_tokenizer_jni.cc",
    ],
    copts = tflite_copts(),
    linkopts = [
        "-lm",
        "-ldl",
    ],
    deps = [
        ":bert_tokenizer",
        ":tokenizer_jni_lib",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "@com_google_absl//absl/memory",
        "@org_tensorflow//tensorflow/lite/java/jni",
    ],
    alwayslink = 1,
)

tflite_jni_binary(
    name = "libbert_tokenizer_jni.so",
    deps = [
        ":bert_tokenizer_jni_lib",
    ],
)

cc_library(
    name = "bert_tokenizer_runtime",
    srcs = ["libbert_tokenizer_jni.so"],
    alwayslink = 1,
)

android_library(
    name = "bert_tokenizer_jni",
    custom_package = "org.tensorflow.lite.support.text",
    manifest = "DummyManifest.xml",
    resource_files = [],
    deps = [
        ":bert_tokenizer_runtime",  # build_cleaner: skip
    ],
)

cc_library(
    name = "sentencepiece_tokenizer",
    hdrs = [
        "sentencepiece_tokenizer.h",
    ],
    deps = [
        ":tokenizer",
        "@com_google_sentencepiece//src:sentencepiece_processor",
    ],
)

cc_library(
    name = "sentencepiece_jni_lib",
    srcs = [
        "sentencepiece_jni.cc",
    ],
    copts = tflite_copts(),
    linkopts = [
        "-lm",
        "-ldl",
    ],
    deps = [
        ":sentencepiece_tokenizer",
        ":tokenizer_jni_lib",
        "//tensorflow_lite_support/cc/utils:jni_utils",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/strings",
        "@org_tensorflow//tensorflow/lite/java/jni",
    ],
    alwayslink = 1,
)

cc_library(
    name = "sentencepiece_runtime",
    srcs = ["libsentencepiece_jni.so"],
    alwayslink = 1,
)

tflite_jni_binary(
    name = "libsentencepiece_jni.so",
    deps = [
        ":sentencepiece_jni_lib",
    ],
)

android_library(
    name = "sentencepiece_jni",
    custom_package = "org.tensorflow.lite.support.text",
    manifest = "DummyManifest.xml",
    resource_files = [],
    deps = [
        ":sentencepiece_runtime",  # build_cleaner: skip
    ],
)

cc_library(
    name = "tokenizer_utils",
    srcs = ["tokenizer_utils.cc"],
    hdrs = [
        "tokenizer_utils.h",
    ],
    deps = [
        ":bert_tokenizer",
        ":regex_tokenizer",
        ":sentencepiece_tokenizer",
        ":tokenizer",
        "//tensorflow_lite_support/cc:common",
        "//tensorflow_lite_support/cc/port:status_macros",
        "//tensorflow_lite_support/cc/port:statusor",
        "//tensorflow_lite_support/metadata:metadata_schema_cc",
        "//tensorflow_lite_support/metadata/cc:metadata_extractor",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:string_view",
    ],
)

cc_library(
    name = "regex_tokenizer",
    srcs = [
        "regex_tokenizer.cc",
    ],
    hdrs = [
        "regex_tokenizer.h",
    ],
    deps = [
        ":tokenizer",
        "//tensorflow_lite_support/cc/utils:common_utils",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/strings",
        "@com_googlesource_code_re2//:re2",
    ],
)
