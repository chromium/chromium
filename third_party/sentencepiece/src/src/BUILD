package(
    default_visibility = ["//visibility:public"],
    features = [
        "-layering_check",
        "-parse_headers",
    ],
)

licenses(["notice"])  # Apache 2, BSD, MIT

proto_library(
    name = "sentencepiece_proto",
    srcs = ["sentencepiece.proto"],
)

cc_proto_library(
    name = "sentencepiece_cc_proto",
    deps = [":sentencepiece_proto"],
)

proto_library(
    name = "sentencepiece_model_proto",
    srcs = ["sentencepiece_model.proto"],
)

cc_proto_library(
    name = "sentencepiece_model_cc_proto",
    deps = [":sentencepiece_model_proto"],
)

cc_library(
    name = "common",
    hdrs = ["common.h"],
    deps = [
        "//src/deps:basic_types",
        "//src/deps:status",
        "@com_google_absl//absl/base",
    ],
)

cc_library(
    name = "sentencepiece_processor",
    srcs = [
        "bpe_model.cc",
        "char_model.cc",
        "filesystem.cc",
        "model_factory.cc",
        "model_interface.cc",
        "normalizer.cc",
        "sentencepiece_processor.cc",
        "unigram_model.cc",
        "util.cc",
        "word_model.cc",
    ],
    hdrs = [
        "bpe_model.h",
        "char_model.h",
        "filesystem.h",
        "freelist.h",
        "model_factory.h",
        "model_interface.h",
        "normalizer.h",
        "sentencepiece_processor.h",
        "trainer_interface.h",
        "unigram_model.h",
        "util.h",
        "word_model.h",
    ],
    deps =
        [
            ":common",
            ":sentencepiece_cc_proto",
            ":sentencepiece_model_cc_proto",
            "@com_github_gflags_gflags//:gflags",
            "@com_google_glog//:glog",
            "@com_google_googletest//:gtest",
            "@com_google_absl//absl/memory",
            "@com_google_absl//absl/strings",
            "@com_google_absl//absl/container:flat_hash_map",
            "@com_google_absl//absl/container:flat_hash_set",
            "//third_party/darts_clone",
            "//src/deps:threadpool",
            "//src/deps:status",
        ],
)

cc_library(
    name = "sentencepiece_trainer",
    srcs = [
        "bpe_model_trainer.cc",
        "builder.cc",
        "char_model_trainer.cc",
        "sentencepiece_trainer.cc",
        "trainer_factory.cc",
        "trainer_interface.cc",
        "unicode_script.cc",
        "unigram_model_trainer.cc",
        "word_model_trainer.cc",
    ],
    hdrs = [
        "bpe_model_trainer.h",
        "builder.h",
        "char_model_trainer.h",
        "normalization_rule.h",
        "sentencepiece_trainer.h",
        "spec_parser.h",
        "trainer_factory.h",
        "trainer_interface.h",
        "unicode_script.h",
        "unicode_script_map.h",
        "unigram_model_trainer.h",
        "word_model_trainer.h",
    ],
    deps = [
        ":pretokenizer_for_training",
        ":sentencepiece_processor",
        "//third_party/esaxx",
        "@com_google_absl//absl/container:node_hash_map",
        "@com_google_absl//absl/container:node_hash_set",
        "@com_google_absl//absl/memory",
        "@com_google_glog//:glog",
    ],
)

cc_library(
    name = "pretokenizer_for_training",
    srcs = ["pretokenizer_for_training.cc"],
    hdrs = ["pretokenizer_for_training.h"],
    deps = [
        ":common",
        ":sentencepiece_cc_proto",
        "@com_google_absl//absl/strings",
        "@com_google_glog//:glog",
    ],
)

cc_test(
    name = "bpe_model_test",
    srcs = [
        "bpe_model_test.cc",
    ],
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "bpe_model_trainer_test",
    srcs = [
        "bpe_model_trainer_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "builder_test",
    srcs = [
        "builder_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "char_model_test",
    srcs = [
        "char_model_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "char_model_trainer_test",
    srcs = [
        "char_model_trainer_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "filesystem_test",
    srcs = [
        "filesystem_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "model_factory_test",
    srcs = [
        "model_factory_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "model_interface_test",
    srcs = [
        "model_interface_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "sentencepiece_processor_test",
    srcs = [
        "sentencepiece_processor_test.cc",
    ],
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "unicode_script_test",
    srcs = [
        "unicode_script_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "unigram_model_test",
    srcs = [
        "unigram_model_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "unigram_model_trainer_test",
    srcs = [
        "unigram_model_trainer_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "util_test",
    srcs = [
        "util_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "word_model_test",
    srcs = [
        "word_model_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "normalizer_test",
    srcs = [
        "normalizer_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "word_model_trainer_test",
    srcs = [
        "word_model_trainer_test.cc",
    ],
    data = glob([
        "test_data/*",
    ]),
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_test(
    name = "pretokenizer_for_training_test",
    srcs = [
        "pretokenizer_for_training_test.cc",
    ],
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
        "@com_google_glog//:glog",
        "@com_google_googletest//:gtest_main",
    ],
)

cc_binary(
    name = "spm_encode",
    srcs = ["spm_encode_main.cc"],
    deps = [":sentencepiece_processor"],
)

cc_binary(
    name = "spm_decode",
    srcs = ["spm_decode_main.cc"],
    deps = [":sentencepiece_processor"],
)

cc_binary(
    name = "spm_train",
    srcs = ["spm_train_main.cc"],
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
    ],
)

cc_binary(
    name = "spm_normalize",
    srcs = ["spm_normalize_main.cc"],
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
    ],
)

cc_binary(
    name = "spm_export_vocab",
    srcs = ["spm_export_vocab_main.cc"],
    deps = [":sentencepiece_processor"],
)

cc_binary(
    name = "compile_charsmap",
    srcs = ["compile_charsmap_main.cc"],
    deps = [
        ":sentencepiece_processor",
        ":sentencepiece_trainer",
    ],
)

filegroup(
    name = "test_data",
    data = glob([
        "test_data/*.model",
    ]),
)
