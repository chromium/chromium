# Kernels for tf.text ops.

# [internal] load cc_proto_library.bzl
load("//tensorflow_text:tftext.bzl", "tf_deps", "tf_text_kernel_library")

licenses(["notice"])

# Visibility rules
package(default_visibility = ["//visibility:public"])

exports_files(["LICENSE"])

cc_library(
    name = "constrained_sequence",
    srcs = ["constrained_sequence.cc"],
    hdrs = ["constrained_sequence.h"],
    deps = tf_deps(deps = [
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
        # tf:protos_all_cc tensorflow dep,
    ]),
)

tf_text_kernel_library(
    name = "constrained_sequence_kernel",
    srcs = ["constrained_sequence_kernel.cc"],
    deps = [
        ":constrained_sequence",
        "@com_google_absl//absl/base:core_headers",
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
        # tf:protos_all_cc tensorflow dep,
    ]),
)

cc_test(
    name = "constrained_sequence_kernel_input_validation_test",
    srcs = ["constrained_sequence_kernel_input_validation_test.cc"],
    deps = [
        ":text_kernels_test_util",
        "@com_google_googletest//:gtest_main",
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
        # tf:protos_all_cc tensorflow dep,
        # tf:test tensorflow dep,
        # tf:testlib tensorflow dep,
        # tf/kernels:ops_testutil tensorflow dep,
        "//tensorflow_text:constrained_sequence_op_cc",
    ],
)

cc_test(
    name = "exp_greedy_constrained_sequence_kernel_test",
    srcs = ["exp_greedy_constrained_sequence_kernel_test.cc"],
    deps = [
        ":text_kernels_test_util",
        "@com_google_googletest//:gtest_main",
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
        # tf:protos_all_cc tensorflow dep,
        # tf:test tensorflow dep,
        # tf:testlib tensorflow dep,
        # tf/kernels:ops_testutil tensorflow dep,
        "//tensorflow_text:constrained_sequence_op_cc",
    ],
)

cc_test(
    name = "exp_viterbi_constrained_sequence_kernel_test",
    srcs = ["exp_viterbi_constrained_sequence_kernel_test.cc"],
    deps = [
        ":text_kernels_test_util",
        "@com_google_googletest//:gtest_main",
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
        # tf:protos_all_cc tensorflow dep,
        # tf:test tensorflow dep,
        # tf:testlib tensorflow dep,
        # tf/kernels:ops_testutil tensorflow dep,
        "//tensorflow_text:constrained_sequence_op_cc",
    ],
)

cc_test(
    name = "log_greedy_constrained_sequence_kernel_test",
    srcs = ["log_greedy_constrained_sequence_kernel_test.cc"],
    deps = [
        ":text_kernels_test_util",
        "@com_google_googletest//:gtest_main",
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
        # tf:protos_all_cc tensorflow dep,
        # tf:test tensorflow dep,
        # tf:testlib tensorflow dep,
        # tf/kernels:ops_testutil tensorflow dep,
        "//tensorflow_text:constrained_sequence_op_cc",
    ],
)

cc_test(
    name = "log_viterbi_constrained_sequence_kernel_test",
    srcs = ["log_viterbi_constrained_sequence_kernel_test.cc"],
    deps = [
        ":text_kernels_test_util",
        "@com_google_googletest//:gtest_main",
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
        # tf:protos_all_cc tensorflow dep,
        # tf:test tensorflow dep,
        # tf:testlib tensorflow dep,
        # tf/kernels:ops_testutil tensorflow dep,
        "//tensorflow_text:constrained_sequence_op_cc",
    ],
)

cc_library(
    name = "disjoint_set_forest",
    hdrs = ["disjoint_set_forest.h"],
    deps = tf_deps(deps = [
        # tf:lib tensorflow dep,
    ]),
)

cc_test(
    name = "disjoint_set_forest_test",
    size = "small",
    srcs = ["disjoint_set_forest_test.cc"],
    deps = [
        ":disjoint_set_forest",
        "@com_google_googletest//:gtest_main",
    ],
)

tf_text_kernel_library(
    name = "mst_op_kernels",
    srcs = ["mst_op_kernels.cc"],
    deps = [
        ":mst_solver",
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
    ]),
)

cc_library(
    name = "mst_solver",
    hdrs = ["mst_solver.h"],
    deps = [
        ":disjoint_set_forest",
        # absl/types:span dep
        # absl/strings dep
    ] + tf_deps(deps = [
        # tf:lib tensorflow dep,
    ]),
)

cc_test(
    name = "mst_solver_test",
    size = "small",
    srcs = ["mst_solver_test.cc"],
    deps = [
        ":mst_solver",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        # tf:test tensorflow dep,
    ],
)

cc_test(
    name = "mst_solver_random_comparison_test",
    size = "small",
    timeout = "long",
    srcs = ["mst_solver_random_comparison_test.cc"],
    tags = [
        "nofastbuild",  # exclude from non-opt TAP projects
        "optonly",  # exclude from non-opt TAP projects
    ],
    deps = [
        ":mst_solver",
        ":spanning_tree_iterator",
        "@com_google_googletest//:gtest",  # google-only
        "@com_google_googletest//:gtest_main",
        "@com_google_absl//absl/flags:flag",
        # tf:lib tensorflow dep,
    ],
)

proto_library(
    name = "edit_changes_proto",
    srcs = ["edit_changes.proto"],
)

cc_proto_library(
    name = "edit_changes_cc_proto",
    deps = [":edit_changes_proto"],
)

tf_text_kernel_library(
    name = "normalize_kernels",
    srcs = ["normalize_kernels.cc"],
    deps = [
        ":edit_changes_cc_proto",
        "//third_party/icu/data:icu_normalization_data",
        # absl/strings dep
        "@icu//:nfkc",
        "@icu//:nfkc_cf",
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
    ]),
)

cc_library(
    name = "regex_split",
    srcs = ["regex_split.cc"],
    hdrs = ["regex_split.h"],
    deps = [
        # absl/strings dep
        "@com_googlesource_code_re2//:re2",
    ] + tf_deps(),
)

tf_text_kernel_library(
    name = "regex_split_kernels",
    srcs = ["regex_split_kernels.cc"],
    deps = [
        ":regex_split",
        "@com_google_absl//absl/memory",
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
    ]),
)

cc_test(
    name = "regex_split_test",
    srcs = ["regex_split_test.cc"],
    deps = [
        ":regex_split",
        "@com_google_googletest//:gtest_main",
        # absl/strings dep
        "@com_googlesource_code_re2//:re2",
        # tf:lib tensorflow dep,
    ],
)

tf_text_kernel_library(
    name = "rouge_l_kernel",
    srcs = ["rouge_l_kernel.cc"],
    deps = [
        # absl/strings dep
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
    ]),
)

cc_test(
    name = "rouge_l_kernel_test",
    size = "small",
    srcs = ["rouge_l_kernel_test.cc"],
    deps = [
        ":rouge_l_kernel",
        # tf:framework tensorflow dep,
        # tf:test tensorflow dep,
        # tf:test_main tensorflow dep,
        # tf:testlib tensorflow dep,
        # tf/kernels:ops_testutil tensorflow dep,
        "//tensorflow_text:text_similarity_metric_ops_cc",
    ],
)

tf_text_kernel_library(
    name = "sentence_breaking_kernels",
    srcs = ["sentence_breaking_kernels.cc"],
    deps = [
        ":sentence_breaking_utils",
        ":sentence_fragmenter",
        # absl/strings dep
        "@icu//:common",
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
    ]),
)

cc_library(
    name = "sentence_breaking_utils",
    srcs = ["sentence_breaking_utils.cc"],
    hdrs = ["sentence_breaking_utils.h"],
    deps = [
        # absl/strings dep
        "@icu//:common",
    ] + tf_deps(deps = [
        # tf:lib tensorflow dep,
    ]),
)

cc_test(
    name = "sentence_breaking_utils_test",
    size = "small",
    srcs = ["sentence_breaking_utils_test.cc"],
    deps = [
        ":sentence_breaking_utils",
        "@com_google_googletest//:gtest",
        "@com_google_googletest//:gtest_main",
        "@icu//:common",
    ],
)

cc_library(
    name = "sentence_fragmenter",
    srcs = ["sentence_fragmenter.cc"],
    hdrs = ["sentence_fragmenter.h"],
    deps = [
        ":sentence_breaking_utils",
    ] + tf_deps(deps = [
        # tf:lib tensorflow dep,
    ]),
)

tf_text_kernel_library(
    name = "sentence_breaking_kernels_v2",
    srcs = ["sentence_breaking_kernels_v2.cc"],
    deps = [
        ":sentence_fragmenter_v2",
        # absl/strings dep
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
    ]),
)

cc_library(
    name = "sentence_fragmenter_v2",
    srcs = ["sentence_fragmenter_v2.cc"],
    hdrs = ["sentence_fragmenter_v2.h"],
    deps = [
        # absl/strings dep
        "@icu//:common",
    ] + tf_deps(deps = [
        # tf:lib tensorflow dep,
    ]),
)

cc_test(
    name = "sentence_fragmenter_v2_test",
    srcs = ["sentence_fragmenter_v2_test.cc"],
    deps = [
        ":sentence_fragmenter_v2",
        "@com_google_googletest//:gtest_main",
        # absl/strings dep
        "@icu//:common",
    ],
)

tf_text_kernel_library(
    name = "sentencepiece_kernels",
    srcs = ["sentencepiece_kernels.cc"],
    deps = [
        "@com_google_absl//absl/base:core_headers",
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/meta:type_traits",
        # absl/strings dep
        # absl/types:span dep
        "@com_google_sentencepiece//src:sentencepiece_cc_proto",
        "@com_google_sentencepiece//src:sentencepiece_processor",
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
        # tf:framework_headers_lib tensorflow dep,
        # tf:lib tensorflow dep,
        # tf:protos_all_cc tensorflow dep,
    ]),
)

cc_library(
    name = "spanning_tree_iterator",
    testonly = 1,
    srcs = ["spanning_tree_iterator.cc"],
    hdrs = ["spanning_tree_iterator.h"],
    deps = [
        # tf:lib tensorflow dep,
    ] + tf_deps(),
)

cc_test(
    name = "spanning_tree_iterator_test",
    size = "small",
    srcs = ["spanning_tree_iterator_test.cc"],
    deps = [
        ":spanning_tree_iterator",
        "@com_google_googletest//:gtest_main",
        # tf:lib tensorflow dep,
    ],
)

tf_text_kernel_library(
    name = "split_merge_tokenize_kernel",
    srcs = ["split_merge_tokenize_kernel.cc"],
    deps = [
        "@icu//:common",
        # absl/strings dep
        "@com_google_absl//absl/base:core_headers",
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
    ]),
)

cc_library(
    name = "text_kernels_test_util",
    testonly = 1,
    srcs = ["text_kernels_test_util.cc"],
    hdrs = ["text_kernels_test_util.h"],
    deps = [
        "@com_google_googletest//:gtest",
        # tf:framework tensorflow dep,
        # tf:testlib tensorflow dep,
    ],
)

tf_text_kernel_library(
    name = "tokenizer_from_logits_kernel",
    srcs = ["tokenizer_from_logits_kernel.cc"],
    deps = [
        "@icu//:common",
        # absl/strings dep
        "@com_google_absl//absl/base:core_headers",
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
    ]),
)

tf_text_kernel_library(
    name = "unicode_script_tokenize_kernel",
    srcs = ["unicode_script_tokenize_kernel.cc"],
    deps = [
        "@icu//:common",
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
    ]),
)

cc_test(
    name = "unicode_script_tokenize_kernel_test",
    srcs = ["unicode_script_tokenize_kernel_test.cc"],
    deps = [
        ":text_kernels_test_util",
        "@com_google_googletest//:gtest_main",
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
        # tf:test tensorflow dep,
        # tf:testlib tensorflow dep,
        # tf/kernels:ops_testutil tensorflow dep,
        "//tensorflow_text:unicode_script_tokenizer_cc",
    ],
)

tf_text_kernel_library(
    name = "whitespace_tokenize_kernel",
    srcs = ["whitespace_tokenize_kernel.cc"],
    deps = [
        "@icu//:common",
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
    ]),
)

cc_test(
    name = "whitespace_tokenize_kernel_test",
    srcs = ["whitespace_tokenize_kernel_test.cc"],
    deps = [
        ":text_kernels_test_util",
        "@com_google_googletest//:gtest_main",
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
        # tf:test tensorflow dep,
        # tf:testlib tensorflow dep,
        # tf/kernels:ops_testutil tensorflow dep,
        "//tensorflow_text:whitespace_tokenizer_cc",
    ],
)

tf_text_kernel_library(
    name = "wordpiece_kernel",
    srcs = ["wordpiece_kernel.cc"],
    deps = [
        ":wordpiece_tokenizer",
        "@com_google_absl//absl/base:core_headers",
    ] + tf_deps(deps = [
        # tf:framework tensorflow dep,
        # tf:lib tensorflow dep,
    ]),
)

cc_library(
    name = "wordpiece_tokenizer",
    srcs = ["wordpiece_tokenizer.cc"],
    hdrs = ["wordpiece_tokenizer.h"],
    deps = [
        # absl/strings dep
        "@icu//:common",
    ] + tf_deps(),
)

cc_test(
    name = "wordpiece_kernel_test",
    size = "small",
    srcs = ["wordpiece_kernel_test.cc"],
    deps = [
        ":wordpiece_kernel",
        # tf:framework tensorflow dep,
        # tf:test tensorflow dep,
        # tf:test_main tensorflow dep,
        # tf:testlib tensorflow dep,
        # tf/kernels:ops_testutil tensorflow dep,
        "//tensorflow_text:wordpiece_tokenizer_cc",
    ],
)
