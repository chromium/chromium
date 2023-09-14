# Memorymappable, WASM compilable, implementation of the encoder.
#

load("@flatbuffers//:build_defs.bzl", "flatbuffer_cc_library")
load("@org_tensorflow//tensorflow:tensorflow.bzl", "tf_gen_op_wrapper_py")
load(":native.bzl", "micore_tf_copts", "micore_tf_deps")

package(
    default_visibility = [
        "//tensorflow_lite_support:users",
    ],
    licenses = ["notice"],  # Apache 2.0
)

filegroup(
    name = "testdata",
    srcs = glob([
        "testdata/**",
    ]),
)

filegroup(
    name = "config_fbs",
    srcs = ["config.fbs"],
)

flatbuffer_cc_library(
    name = "config",
    srcs = [
        "config.fbs",
    ],
)

flatbuffer_cc_library(
    name = "encoder_config",
    srcs = [
        "encoder_config.fbs",
    ],
    includes = [":config_fbs"],
)

flatbuffer_cc_library(
    name = "decoder_config",
    srcs = [
        "decoder_config.fbs",
    ],
    includes = [":config_fbs"],
)

cc_library(
    name = "utils",
    srcs = [
    ],
    hdrs = [
        "utils.h",
    ],
)

cc_library(
    name = "double_array_trie",
    srcs = [
    ],
    hdrs = [
        "double_array_trie.h",
    ],
    deps = [
        ":config",
        ":utils",
    ],
)

cc_library(
    name = "double_array_trie_builder",
    srcs = [
        "double_array_trie_builder.cc",
    ],
    hdrs = [
        "double_array_trie_builder.h",
    ],
    deps = [
        ":config",
        ":utils",
        "@darts_clone",
    ],
)

cc_test(
    name = "double_array_trie_test",
    srcs = [
        "double_array_trie_test.cc",
    ],
    deps = [
        ":double_array_trie",
        ":double_array_trie_builder",
        ":encoder_config",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_library(
    name = "sentencepiece_constants",
    srcs = [],
    hdrs = ["sentencepiece_constants.h"],
)

cc_library(
    name = "model_converter",
    srcs = [
        "model_converter.cc",
    ],
    hdrs = [
        "model_converter.h",
    ],
    deps = [
        ":config",
        ":decoder_config",
        ":double_array_trie_builder",
        ":encoder_config",
        ":sentencepiece_constants",
        "//tensorflow_lite_support/cc/port:statusor",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings",
        "@com_google_sentencepiece//src:sentencepiece_model_cc_proto",
    ],
)

cc_library(
    name = "optimized_encoder",
    srcs = [
        "optimized_encoder.cc",
    ],
    hdrs = [
        "optimized_encoder.h",
    ],
    deps = [
        ":config",
        ":double_array_trie",
        ":encoder_config",
    ],
)

cc_library(
    name = "optimized_decoder",
    srcs = [
        "optimized_decoder.cc",
    ],
    hdrs = [
        "optimized_decoder.h",
    ],
    deps = [
        "config",
        ":decoder_config",
        ":double_array_trie",
    ],
)

cc_library(
    name = "sentencepiece_tokenizer_h",
    hdrs = [
        "sentencepiece_tokenizer.h",
    ],
)

cc_library(
    name = "sentencepiece_detokenizer_h",
    hdrs = [
        "sentencepiece_detokenizer.h",
    ],
)

cc_library(
    name = "sentencepiece_tokenizer_op",
    srcs = ["sentencepiece_tokenizer_op.cc"],
    copts = micore_tf_copts(),
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":optimized_encoder",
        ":sentencepiece_tokenizer_h",
    ] + micore_tf_deps(),
    alwayslink = 1,
)

cc_binary(
    name = "sentencepiece_tokenizer_op.so",
    srcs = [
        "sentencepiece_tokenizer_op.cc",
    ],
    copts = micore_tf_copts(),
    linkshared = 1,
    deps = [
        ":optimized_encoder",
        ":sentencepiece_tokenizer_h",
    ] + micore_tf_deps(),
)

cc_library(
    name = "sentencepiece_detokenizer_op",
    srcs = ["sentencepiece_detokenizer_op.cc"],
    copts = micore_tf_copts(),
    visibility = [
        "//visibility:public",
    ],
    deps = [
        ":optimized_decoder",
        ":sentencepiece_detokenizer_h",
    ] + micore_tf_deps(),
    alwayslink = 1,
)

cc_binary(
    name = "sentencepiece_detokenizer_op.so",
    srcs = [
        "sentencepiece_detokenizer_op.cc",
    ],
    copts = micore_tf_copts(),
    linkshared = 1,
    deps = [
        ":optimized_decoder",
        ":sentencepiece_detokenizer_h",
    ] + micore_tf_deps(),
)

cc_library(
    name = "sentencepiece_tokenizer_tflite",
    srcs = ["sentencepiece_tokenizer_tflite.cc"],
    visibility = [
        "//visibility:public",
    ],
    deps =
        [
            ":optimized_encoder",
            ":sentencepiece_tokenizer_h",
            "@flatbuffers",
            "@org_tensorflow//tensorflow/lite:framework",
            "@org_tensorflow//tensorflow/lite:string_util",
            "@org_tensorflow//tensorflow/lite/c:common",
            "@org_tensorflow//tensorflow/lite/kernels:kernel_util",
            "@org_tensorflow//tensorflow/lite/kernels/internal:tensor",
        ],
)

cc_library(
    name = "sentencepiece_detokenizer_tflite",
    srcs = ["sentencepiece_detokenizer_tflite.cc"],
    visibility = [
        "//visibility:public",
    ],
    deps =
        [
            ":optimized_decoder",
            ":sentencepiece_detokenizer_h",
            "@flatbuffers",
            "@org_tensorflow//tensorflow/lite:framework",
            "@org_tensorflow//tensorflow/lite:string_util",
            "@org_tensorflow//tensorflow/lite/c:common",
            "@org_tensorflow//tensorflow/lite/kernels:kernel_util",
            "@org_tensorflow//tensorflow/lite/kernels/internal:tensor",
        ],
)

cc_test(
    name = "optimized_encoder_test",
    srcs = [
        "optimized_encoder_test.cc",
    ],
    data = [
        ":testdata",
    ],
    deps = [
        ":double_array_trie_builder",
        ":encoder_config",
        ":model_converter",
        ":optimized_encoder",
        "//tensorflow_lite_support/cc/test:test_utils",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
        "@com_google_sentencepiece//src:sentencepiece_cc_proto",
        "@com_google_sentencepiece//src:sentencepiece_processor",
        "@org_tensorflow//tensorflow/core:lib",
    ],
)

cc_test(
    name = "optimized_decoder_test",
    srcs = [
        "optimized_decoder_test.cc",
    ],
    data = [
        ":testdata",
    ],
    deps = [
        ":decoder_config",
        ":double_array_trie_builder",
        ":model_converter",
        ":optimized_decoder",
        "//tensorflow_lite_support/cc/test:test_utils",
        "@com_google_absl//absl/flags:flag",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_googletest//:gtest_main",
        "@com_google_sentencepiece//src:sentencepiece_cc_proto",
        "@com_google_sentencepiece//src:sentencepiece_processor",
        "@org_tensorflow//tensorflow/core:lib",
    ],
)

cc_library(
    name = "py_tflite_registerer",
    srcs = ["py_tflite_registerer.cc"],
    hdrs = ["py_tflite_registerer.h"],
    deps = [
        ":sentencepiece_detokenizer_tflite",
        ":sentencepiece_tokenizer_tflite",
        "@org_tensorflow//tensorflow/lite:framework",
        "@org_tensorflow//tensorflow/lite/kernels:builtin_ops",
    ],
    alwayslink = 1,
)

config_setting(
    name = "armeabi_v7a_and_fastbuild",
    values = {
        "cpu": "armeabi-v7a",
        "compilation_mode": "fastbuild",
    },
    visibility = ["//visibility:public"],
)

config_setting(
    name = "armeabi_v7a_and_dbg",
    values = {
        "cpu": "armeabi-v7a",
        "compilation_mode": "dbg",
    },
    visibility = ["//visibility:public"],
)

config_setting(
    name = "android",
    values = {"crosstool_top": "//external:android/crosstool"},
    visibility = ["//visibility:public"],
)

config_setting(
    name = "macos_i386",
    values = {
        "apple_platform_type": "macos",
        "cpu": "darwin",
    },
    visibility = ["//visibility:public"],
)

config_setting(
    name = "macos_x86_64",
    values = {
        "apple_platform_type": "macos",
        "cpu": "darwin_x86_64",
    },
    visibility = ["//visibility:public"],
)

alias(
    name = "macos",
    actual = select({
        ":macos_i386": ":macos_i386",
        ":macos_x86_64": ":macos_x86_64",
        "//conditions:default": ":macos_i386",  # Arbitrarily chosen from above.
    }),
    visibility = ["//visibility:public"],
)

config_setting(
    name = "ios",
    values = {
        "crosstool_top": "@bazel_tools//tools/cpp:toolchain",
        "apple_platform_type": "ios",
    },
    visibility = ["//visibility:public"],
)

alias(
    name = "apple",
    actual = select({
        ":macos": ":macos",
        ":ios": ":ios",
        "//conditions:default": ":ios",  # Arbitrarily chosen from above.
    }),
    visibility = ["//visibility:public"],
)
