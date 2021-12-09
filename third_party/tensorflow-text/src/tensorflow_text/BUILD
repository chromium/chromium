load("//tensorflow_text:tftext.bzl", "py_tf_text_library")

# [internal] load build_test.bzl
load("@org_tensorflow//tensorflow/lite:build_def.bzl", "tflite_cc_shared_object")

# Visibility rules
package(
    default_visibility = ["//visibility:public"],
)

licenses(["notice"])

exports_files(["LICENSE"])

####################################
# Public TF.Text library to import #
####################################

# Most TF users will want to add this as their dependency.
alias(
    name = "tensorflow_text",
    actual = ":ops",
    visibility = ["//visibility:public"],
)

cc_library(
    name = "ops_lib",
    visibility = ["//visibility:public"],
    deps = [
        ":constrained_sequence_op_cc",
        ":fast_wordpiece_tokenizer_cc",
        ":mst_ops_cc",
        ":normalize_ops_cc",
        ":regex_split_ops_cc",
        ":sentence_breaking_ops_cc",
        ":sentencepiece_tokenizer_cc",
        ":split_merge_from_logits_tokenizer_cc",
        ":split_merge_tokenizer_cc",
        ":state_based_sentence_breaker_op_cc",
        ":text_similarity_metric_ops_cc",
        ":unicode_script_tokenizer_cc",
        ":whitespace_tokenizer_cc",
        ":whitespace_tokenizer_v2_cc",
        ":wordpiece_tokenizer_cc",
    ],
)

tflite_cc_shared_object(
    name = "libtftextops_for_testing.so",
    deps = [":ops_lib"],
)

py_library(
    name = "ops",
    srcs = [
        "__init__.py",
        "python/__init__.py",
        "python/keras/__init__.py",
        "python/keras/layers/__init__.py",
        "python/metrics/__init__.py",
        "python/numpy/__init__.py",
        "python/ops/__init__.py",
        "tools/__init__.py",
    ],
    srcs_version = "PY3",
    deps = [
        ":bert_tokenizer",
        ":create_feature_bitmask_op",
        ":fast_wordpiece_tokenizer",
        ":greedy_constrained_sequence_op",
        ":hub_module_splitter",
        ":hub_module_tokenizer",
        ":item_selector_ops",
        ":masking_ops",
        ":mst_ops",
        ":ngrams_op",
        ":normalize_ops",
        ":pad_along_dimension_op",
        ":pad_model_inputs_ops",
        ":pointer_ops",
        ":regex_split_ops",
        ":segment_combiner_ops",
        ":sentence_breaking_ops",
        ":sentencepiece_tokenizer",
        ":sliding_window_op",
        ":split_merge_from_logits_tokenizer",
        ":split_merge_tokenizer",
        ":splitter",
        ":state_based_sentence_breaker_op",
        ":string_ops",
        ":text_similarity_metric_ops",
        ":todense_layer",
        ":tokenization",
        ":tokenization_layers",
        ":trimmer_ops",
        ":unicode_char_tokenizer",
        ":unicode_script_tokenizer",
        ":viterbi_constrained_sequence_op",
        ":viterbi_decode",
        ":whitespace_tokenizer",
        ":whitespace_tokenizer_v2",
        ":wordpiece_tokenizer",
        ":wordshape_ops",
        # python:util tensorflow dep,
        "//tensorflow_text/core/pybinds:tflite_registrar",
        "//tensorflow_text/tools/wordpiece_vocab",
    ],
)

# build_test

# public_names_test

# This is required for other external users to build tests using these models.
exports_files(
    glob(["python/ops/test_data/**"]),
)

##########################
# Individual tf.text ops #
##########################
# The py libraries are ordered alphabetically and are grouped with their corresponding tests.

py_library(
    name = "bert_tokenizer",
    srcs = ["python/ops/bert_tokenizer.py"],
    tags = ["ignore_srcs"],
    deps = [
        ":normalize_ops",
        ":regex_split_ops",
        ":tokenization",
        ":wordpiece_tokenizer",
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/eager:monitoring tensorflow dep,
    ],
)

py_test(
    name = "bert_tokenizer_test",
    size = "small",
    timeout = "moderate",
    srcs = ["python/ops/bert_tokenizer_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":bert_tokenizer",
        "@absl_py//absl/testing:parameterized",
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python:lookup_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
        # python/ops/ragged:ragged_map_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_tf_text_library(
    name = "constrained_sequence_op",
    srcs = [],
    cc_op_defs = ["core/ops/constrained_sequence_op.cc"],
    cc_op_kernels = [
        "//tensorflow_text/core/kernels:constrained_sequence_kernel",
    ],
    deps = [
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python/ops/ragged tensorflow dep,
    ],
)

py_library(
    name = "create_feature_bitmask_op",
    srcs = ["python/ops/create_feature_bitmask_op.py"],
    deps = [
        # python:array_ops tensorflow dep,
        # python:check_ops tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:math_ops tensorflow dep,
    ],
)

py_test(
    name = "create_feature_bitmask_op_test",
    size = "small",
    srcs = ["python/ops/create_feature_bitmask_op_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":create_feature_bitmask_op",
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_test_lib tensorflow dep,
    ],
)

py_tf_text_library(
    name = "fast_wordpiece_tokenizer",
    srcs = ["python/ops/fast_wordpiece_tokenizer.py"],
    cc_op_defs = ["core/ops/fast_wordpiece_tokenizer_op.cc"],
    cc_op_kernels = [
        # lite/kernels/shim:tf_op_shim tensorflow dep,
        "//tensorflow_text/core/kernels:fast_wordpiece_tokenizer_kernel",
    ],
    deps = [
        ":tokenization",
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
        "//tensorflow_text/core/pybinds:pywrap_fast_wordpiece_tokenizer_model_builder",
    ],
)

py_test(
    name = "fast_wordpiece_tokenizer_test",
    size = "small",
    srcs = ["python/ops/fast_wordpiece_tokenizer_test.py"],
    data = [
        ":python/ops/test_data/fast_wordpiece_tokenizer_model.fb",
    ],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":tensorflow_text",
        "@absl_py//absl/testing:parameterized",
        # tensorflow package dep,
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/compat tensorflow dep,
        # python/data/kernel_tests:test_base tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_tf_text_library(
    name = "greedy_constrained_sequence_op",
    srcs = ["python/ops/greedy_constrained_sequence_op.py"],
    cc_op_kernels = [
        ":constrained_sequence_op",
    ],
    deps = [
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "greedy_constrained_sequence_op_test",
    size = "small",
    srcs = ["python/ops/greedy_constrained_sequence_op_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":greedy_constrained_sequence_op",
        #  numpy dep,
        # python:client_testlib tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_library(
    name = "hub_module_splitter",
    srcs = ["python/ops/hub_module_splitter.py"],
    deps = [
        ":splitter",
        "@org_tensorflow_hub//tensorflow_hub",
        # python:array_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "hub_module_splitter_test",
    size = "large",
    srcs = ["python/ops/hub_module_splitter_test.py"],
    data = [
        ":test_data_segmenter_hub_module",
    ],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":ops",
        # python:client_testlib tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python:lookup_ops tensorflow dep,
        # python:variables tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_library(
    name = "hub_module_tokenizer",
    srcs = ["python/ops/hub_module_tokenizer.py"],
    deps = [
        ":hub_module_splitter",
        ":tokenization",
    ],
)

py_test(
    name = "hub_module_tokenizer_test",
    size = "large",
    srcs = ["python/ops/hub_module_tokenizer_test.py"],
    data = [
        ":test_data_segmenter_hub_module",
    ],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":ops",
        # python:client_testlib tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python:lookup_ops tensorflow dep,
        # python:variables tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_library(
    name = "item_selector_ops",
    srcs = ["python/ops/item_selector_ops.py"],
    deps = [
        # python:array_ops tensorflow dep,
        # python:control_flow_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python:lookup_ops tensorflow dep,
        # python:map_fn tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:random_ops tensorflow dep,
        # python:sort_ops tensorflow dep,
        # python/ops/ragged:ragged_array_ops tensorflow dep,
        # python/ops/ragged:ragged_batch_gather_ops tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
        # python/ops/ragged:ragged_functional_ops tensorflow dep,
        # python/ops/ragged:ragged_map_ops tensorflow dep,
        # python/ops/ragged:ragged_math_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
        # python/ops/ragged:ragged_tensor_shape tensorflow dep,
        # python/ops/ragged:ragged_where_op tensorflow dep,
    ],
)

py_test(
    name = "item_selector_ops_test",
    size = "medium",
    srcs = ["python/ops/item_selector_ops_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":item_selector_ops",
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python:math_ops tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_library(
    name = "masking_ops",
    srcs = ["python/ops/masking_ops.py"],
    deps = [
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:map_fn tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:random_ops tensorflow dep,
        # python:sort_ops tensorflow dep,
        # python/ops/ragged:ragged_batch_gather_ops tensorflow dep,
        # python/ops/ragged:ragged_functional_ops tensorflow dep,
        # python/ops/ragged:ragged_map_ops tensorflow dep,
        # python/ops/ragged:ragged_math_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
        # python/ops/ragged:ragged_where_op tensorflow dep,
    ],
)

py_test(
    name = "masking_ops_test",
    size = "medium",
    srcs = ["python/ops/masking_ops_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":item_selector_ops",
        ":masking_ops",
        "@absl_py//absl/testing:parameterized",
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_tf_text_library(
    name = "mst_ops",
    srcs = ["python/ops/mst_ops.py"],
    cc_op_defs = ["core/ops/mst_ops.cc"],
    cc_op_kernels = [
        "//tensorflow_text/core/kernels:mst_op_kernels",
    ],
    deps = [
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:standard_ops tensorflow dep,
    ],
)

py_test(
    name = "mst_ops_test",
    srcs = ["python/ops/mst_ops_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":mst_ops",
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python:math_ops tensorflow dep,
    ],
)

py_library(
    name = "ngrams_op",
    srcs = ["python/ops/ngrams_op.py"],
    deps = [
        ":sliding_window_op",
        # python:errors tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/ops/ragged:ragged_functional_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "ngrams_op_test",
    size = "small",
    srcs = ["python/ops/ngrams_op_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":ngrams_op",
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_tf_text_library(
    name = "normalize_ops",
    srcs = ["python/ops/normalize_ops.py"],
    cc_op_defs = ["core/ops/normalize_ops.cc"],
    cc_op_kernels = [
        "//tensorflow_text/core/kernels:normalize_kernels",
    ],
    deps = [
        # python:dtypes tensorflow dep,
        # python/ops/ragged:ragged_conversion_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "normalize_ops_test",
    size = "small",
    srcs = ["python/ops/normalize_ops_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":normalize_ops",
        # python:client_testlib tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_library(
    name = "pad_along_dimension_op",
    srcs = ["python/ops/pad_along_dimension_op.py"],
    deps = [
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "pad_along_dimension_op_test",
    size = "medium",
    srcs = ["python/ops/pad_along_dimension_op_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":pad_along_dimension_op",
        "@absl_py//absl/testing:parameterized",
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/eager:context tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_library(
    name = "pad_model_inputs_ops",
    srcs = ["python/ops/pad_model_inputs_ops.py"],
    deps = [
        # python:array_ops tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:control_flow_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:tensor_array_ops tensorflow dep,
        # python/ops/ragged:ragged_array_ops tensorflow dep,
        # python/ops/ragged:ragged_map_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "pad_model_inputs_ops_test",
    srcs = ["python/ops/pad_model_inputs_ops_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":pad_model_inputs_ops",
        "@absl_py//absl/testing:parameterized",
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_library(
    name = "pointer_ops",
    srcs = ["python/ops/pointer_ops.py"],
    deps = [
        # python:array_ops tensorflow dep,
        # python:check_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python/ops/ragged:ragged_functional_ops tensorflow dep,
        # python/ops/ragged:ragged_gather_ops tensorflow dep,
        # python/ops/ragged:ragged_math_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
        # python/ops/ragged:ragged_where_op tensorflow dep,
        # python/ops/ragged:segment_id_ops tensorflow dep,
    ],
)

py_test(
    name = "gather_with_default_op_test",
    srcs = ["python/ops/gather_with_default_op_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":pointer_ops",
        "@absl_py//absl/testing:parameterized",
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_test_lib tensorflow dep,
    ],
)

py_test(
    name = "span_alignment_op_test",
    srcs = ["python/ops/span_alignment_op_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":pointer_ops",
        "@absl_py//absl/testing:parameterized",
        # python:client_testlib tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_test(
    name = "span_overlaps_op_test",
    srcs = ["python/ops/span_overlaps_op_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":pointer_ops",
        "@absl_py//absl/testing:parameterized",
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/eager:context tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "ragged_tensor_to_tensor_test",
    srcs = ["python/ragged/ragged_tensor_to_tensor_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":tensorflow_text",
        # tensorflow package dep,
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_tf_text_library(
    name = "regex_split_ops",
    srcs = ["python/ops/regex_split_ops.py"],
    cc_op_defs = ["core/ops/regex_split_ops.cc"],
    cc_op_kernels = ["//tensorflow_text/core/kernels:regex_split_kernels"],
    deps = [
        ":splitter",
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "regex_split_ops_test",
    size = "medium",
    srcs = ["python/ops/regex_split_ops_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":regex_split_ops",
        # python:client_testlib tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python:platform tensorflow dep,
    ],
)

py_library(
    name = "segment_combiner_ops",
    srcs = ["python/ops/segment_combiner_ops.py"],
    deps = [
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:math_ops tensorflow dep,
    ],
)

py_test(
    name = "segment_combiner_ops_test",
    srcs = ["python/ops/segment_combiner_ops_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":segment_combiner_ops",
        "@absl_py//absl/testing:parameterized",
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:dtypes tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_tf_text_library(
    name = "sentence_breaking_ops",
    srcs = ["python/ops/sentence_breaking_ops.py"],
    cc_op_defs = [
        "core/ops/sentence_breaking_ops.cc",
    ],
    cc_op_kernels = [
        "//tensorflow_text/core/kernels:sentence_breaking_kernels",
    ],
    deps = [
        ":regex_split_ops",
        # tf:lib tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/ops/ragged:ragged_map_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "sentence_breaking_ops_test",
    size = "small",
    srcs = ["python/ops/sentence_breaking_ops_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":sentence_breaking_ops",
        "@absl_py//absl/testing:parameterized",
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
        # python/ops/ragged:ragged_map_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_tf_text_library(
    name = "sentencepiece_tokenizer",
    srcs = ["python/ops/sentencepiece_tokenizer.py"],
    cc_op_defs = ["core/ops/sentencepiece_ops.cc"],
    cc_op_kernels = [
        "//tensorflow_text/core/kernels:sentencepiece_kernels",
    ],
    deps = [
        ":tokenization",
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/ops/ragged:ragged_conversion_ops tensorflow dep,
        # python/ops/ragged:ragged_string_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "sentencepiece_tokenizer_test",
    size = "large",
    srcs = ["python/ops/sentencepiece_tokenizer_test.py"],
    data = [
        ":python/ops/test_data/test_oss_model.model",
    ],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":sentencepiece_tokenizer",
        "@absl_py//absl/testing:parameterized",
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
        # python/ops/ragged:ragged_gather_ops tensorflow dep,
        # python/saved_model tensorflow dep,
    ],
)

py_library(
    name = "sliding_window_op",
    srcs = ["python/ops/sliding_window_op.py"],
    deps = [
        # python:array_ops tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "sliding_window_op_test",
    size = "small",
    srcs = ["python/ops/sliding_window_op_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":sliding_window_op",
        "@absl_py//absl/testing:parameterized",
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_tf_text_library(
    name = "split_merge_from_logits_tokenizer",
    srcs = ["python/ops/split_merge_from_logits_tokenizer.py"],
    cc_op_defs = ["core/ops/tokenizer_from_logits_op.cc"],
    cc_op_kernels = [
        "//tensorflow_text/core/kernels:tokenizer_from_logits_kernel",
    ],
    deps = [
        ":tokenization",
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python/ops/ragged tensorflow dep,
    ],
)

py_test(
    name = "split_merge_from_logits_tokenizer_test",
    size = "small",
    srcs = ["python/ops/split_merge_from_logits_tokenizer_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":split_merge_from_logits_tokenizer",
        # python:client_testlib tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_tf_text_library(
    name = "split_merge_tokenizer",
    srcs = ["python/ops/split_merge_tokenizer.py"],
    cc_op_defs = ["core/ops/split_merge_tokenize_op.cc"],
    cc_op_kernels = [
        "//tensorflow_text/core/kernels:split_merge_tokenize_kernel",
    ],
    deps = [
        ":tokenization",
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python/ops/ragged tensorflow dep,
    ],
)

py_test(
    name = "split_merge_tokenizer_test",
    size = "small",
    srcs = ["python/ops/split_merge_tokenizer_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":split_merge_tokenizer",
        # python:client_testlib tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_library(
    name = "splitter",
    srcs = ["python/ops/splitter.py"],
    deps = [
        # python/module tensorflow dep,
    ],
)

py_tf_text_library(
    name = "state_based_sentence_breaker_op",
    srcs = ["python/ops/state_based_sentence_breaker_op.py"],
    cc_op_defs = [
        "core/ops/sentence_breaking_ops_v2.cc",
    ],
    cc_op_kernels = [
        "//tensorflow_text/core/kernels:sentence_breaking_kernels_v2",
    ],
    deps = [
        ":sentence_breaking_ops",
        # tf:lib tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/ops/ragged:ragged_map_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "state_based_sentence_breaker_op_test",
    size = "medium",
    srcs = ["python/ops/state_based_sentence_breaker_op_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":state_based_sentence_breaker_op",
        "@absl_py//absl/testing:parameterized",
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:errors tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
        # python/ops/ragged:ragged_map_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_library(
    name = "string_ops",
    srcs = ["python/ops/string_ops.py"],
    deps = [
        # python:string_ops tensorflow dep,
    ],
)

py_test(
    name = "coerce_to_valid_utf8_op_test",
    size = "small",
    srcs = ["python/ops/coerce_to_valid_utf8_op_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":string_ops",
        # python:client_testlib tensorflow dep,
        # python:framework_test_lib tensorflow dep,
    ],
)

py_tf_text_library(
    name = "text_similarity_metric_ops",
    srcs = ["python/metrics/text_similarity_metric_ops.py"],
    cc_op_defs = ["core/ops/rouge_l_op.cc"],
    cc_op_kernels = ["//tensorflow_text/core/kernels:rouge_l_kernel"],
    deps = [
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:lookup_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "text_similarity_metric_ops_test",
    size = "small",
    srcs = ["python/metrics/text_similarity_metric_ops_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":text_similarity_metric_ops",
        "@absl_py//absl/testing:parameterized",
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python:lookup_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_library(
    name = "todense_layer",
    srcs = ["python/keras/layers/todense.py"],
    deps = [
        # tensorflow package dep,
    ],
)

py_test(
    name = "todense_test",
    size = "large",
    srcs = ["python/keras/layers/todense_test.py"],
    python_version = "PY3",
    shard_count = 2,
    srcs_version = "PY3",
    deps = [
        ":todense_layer",
        "@absl_py//absl/testing:parameterized",
        #  numpy dep,
        # tensorflow package dep,
        # python:framework_test_lib tensorflow dep,
        # python/keras tensorflow dep,
        # python/keras:testing_utils tensorflow dep,
    ],
)

py_library(
    name = "tokenization",
    srcs = ["python/ops/tokenization.py"],
    deps = [
        ":splitter",
        # python/module tensorflow dep,
    ],
)

py_library(
    name = "tokenization_layers",
    srcs = ["python/keras/layers/tokenization_layers.py"],
    srcs_version = "PY2AND3",
    deps = [
        ":unicode_script_tokenizer",
        ":whitespace_tokenizer_v2",
        ":wordpiece_tokenizer",
        # tensorflow package dep,
        # python:lookup_ops tensorflow dep,
        # python/ops/ragged:ragged_conversion_ops tensorflow dep,
    ],
)

py_test(
    name = "tokenization_layers_test",
    size = "large",
    srcs = ["python/keras/layers/tokenization_layers_test.py"],
    python_version = "PY3",
    shard_count = 20,
    deps = [
        ":tokenization_layers",
        "@absl_py//absl/testing:parameterized",
        #  numpy dep,
        # tensorflow package dep,
        # python/keras tensorflow dep,
        # python/keras:testing_utils tensorflow dep,
    ],
)

py_library(
    name = "trimmer_ops",
    srcs = ["python/ops/trimmer_ops.py"],
    deps = [
        ":item_selector_ops",
        # python:array_ops tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:control_flow_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:functional_ops tensorflow dep,
        # python/ops/ragged:ragged_map_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "trimmer_ops_test",
    srcs = ["python/ops/trimmer_ops_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":trimmer_ops",
        "@absl_py//absl/testing:parameterized",
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

filegroup(
    name = "test_data_segmenter_hub_module",
    srcs = glob([
        "python/ops/test_data/segmenter_hub_module/**",
    ]),
)

py_library(
    name = "unicode_char_tokenizer",
    srcs = ["python/ops/unicode_char_tokenizer.py"],
    deps = [
        ":tokenization",
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/ops/ragged:ragged_string_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "unicode_char_tokenizer_test",
    size = "large",
    srcs = ["python/ops/unicode_char_tokenizer_test.py"],
    python_version = "PY3",
    shard_count = 5,
    srcs_version = "PY3",
    deps = [
        ":unicode_char_tokenizer",
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_tf_text_library(
    name = "unicode_script_tokenizer",
    srcs = ["python/ops/unicode_script_tokenizer.py"],
    cc_op_defs = ["core/ops/unicode_script_tokenize_op.cc"],
    cc_op_kernels = [
        "//tensorflow_text/core/kernels:unicode_script_tokenize_kernel",
    ],
    deps = [
        ":tokenization",
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/ops/ragged:ragged_conversion_ops tensorflow dep,
        # python/ops/ragged:ragged_string_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "unicode_script_tokenizer_test",
    size = "large",
    srcs = ["python/ops/unicode_script_tokenizer_test.py"],
    python_version = "PY3",
    shard_count = 5,
    srcs_version = "PY3",
    deps = [
        ":unicode_script_tokenizer",
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_tf_text_library(
    name = "viterbi_constrained_sequence_op",
    srcs = ["python/ops/viterbi_constrained_sequence_op.py"],
    cc_op_kernels = [
        ":constrained_sequence_op",
    ],
    deps = [
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "viterbi_constrained_sequence_op_test",
    size = "small",
    srcs = ["python/ops/viterbi_constrained_sequence_op_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":viterbi_constrained_sequence_op",
        ":viterbi_decode",
        #  numpy dep,
        # python:client_testlib tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_library(
    name = "viterbi_decode",
    srcs = ["python/numpy/viterbi_decode.py"],
    deps = [
        #  numpy dep,
    ],
)

py_test(
    name = "viterbi_decode_test",
    size = "small",
    srcs = ["python/numpy/viterbi_decode_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":viterbi_decode",
        "@absl_py//absl/testing:absltest",
        #  numpy dep,
    ],
)

py_tf_text_library(
    name = "whitespace_tokenizer",
    srcs = [],
    cc_op_defs = ["core/ops/whitespace_tokenize_op.cc"],
    cc_op_kernels = [
        "//tensorflow_text/core/kernels:whitespace_tokenize_kernel",
    ],
    deps = [
        ":tokenization",
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/ops/ragged:ragged_conversion_ops tensorflow dep,
        # python/ops/ragged:ragged_string_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_tf_text_library(
    name = "whitespace_tokenizer_v2",
    srcs = ["python/ops/whitespace_tokenizer.py"],
    cc_op_defs = ["core/ops/whitespace_tokenizer_op.cc"],
    cc_op_kernels = [
        # lite/kernels/shim:tf_op_shim tensorflow dep,
        "//tensorflow_text/core/kernels:whitespace_tokenizer_kernel",
    ],
    deps = [
        ":tokenization",
        ":whitespace_tokenizer",
        # python:array_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/ops/ragged:ragged_conversion_ops tensorflow dep,
        # python/ops/ragged:ragged_string_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
        "//tensorflow_text/core/pybinds:pywrap_whitespace_tokenizer_config_builder",
    ],
)

py_test(
    name = "whitespace_tokenizer_test",
    size = "large",
    srcs = ["python/ops/whitespace_tokenizer_test.py"],
    python_version = "PY3",
    shard_count = 4,
    srcs_version = "PY3",
    deps = [
        ":tensorflow_text",
        # tensorflow package dep,
        # python:client_testlib tensorflow dep,
        # python:constant_op tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_tf_text_library(
    name = "wordpiece_tokenizer",
    srcs = ["python/ops/wordpiece_tokenizer.py"],
    cc_op_defs = ["core/ops/wordpiece_op.cc"],
    cc_op_kernels = [
        "//tensorflow_text/core/kernels:wordpiece_kernel",
    ],
    deps = [
        ":tokenization",
        # python:array_ops tensorflow dep,
        # python:check_ops tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:lookup_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python:sort_ops tensorflow dep,
        # python:string_ops tensorflow dep,
        # python/compat tensorflow dep,
        # python/eager:monitoring tensorflow dep,
        # python/ops/ragged:ragged_functional_ops tensorflow dep,
        # python/ops/ragged:ragged_string_ops tensorflow dep,
        # python/ops/ragged:ragged_tensor tensorflow dep,
    ],
)

py_test(
    name = "wordpiece_tokenizer_test",
    size = "small",
    srcs = ["python/ops/wordpiece_tokenizer_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":wordpiece_tokenizer",
        "@absl_py//absl/testing:parameterized",
        # python:array_ops tensorflow dep,
        # python:client_testlib tensorflow dep,
        # python:dtypes tensorflow dep,
        # python:framework_test_lib tensorflow dep,
        # python:lookup_ops tensorflow dep,
        # python:math_ops tensorflow dep,
        # python/compat tensorflow dep,
        # python/ops/ragged:ragged_factory_ops tensorflow dep,
    ],
)

py_library(
    name = "wordshape_ops",
    srcs = ["python/ops/wordshape_ops.py"],
    deps = [
        # python:array_ops tensorflow dep,
        # python:framework_ops tensorflow dep,
        # python:string_ops tensorflow dep,
    ],
)

py_test(
    name = "wordshape_ops_test",
    size = "small",
    srcs = ["python/ops/wordshape_ops_test.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":wordshape_ops",
        # python:client_testlib tensorflow dep,
        # python:framework_test_lib tensorflow dep,
    ],
)

##############
# Benchmarks #
##############

# To run the benchmarks:
#   bazel run -c opt benchmark_test_name -- --benchmark_filter=.

# The --benchmark_filter flag specifies the list of benchmarks to run using a
# regular expression that is matched against the benchmark methods' name.
# e.g. --benchmark_filter="wordpiece" will run the Wordpiece Tokenizer benchmarks.

py_library(name = "profiler_lib")

py_library(
    name = "benchmark_utils",
    srcs = ["python/benchmarks/benchmark_utils.py"],
    deps = [
        #  numpy dep,
        #  tensorflow datasets dep,
        # python:dtypes tensorflow dep,
        # python:platform tensorflow dep,
        # python:session tensorflow dep,
        # python/data/ops:dataset_ops tensorflow dep,
        # python/eager:context tensorflow dep,
        # python/eager:def_function tensorflow dep,
    ],
)

py_test(
    name = "tokenizers_benchmarks",
    size = "medium",
    srcs = ["python/benchmarks/tokenizers_benchmarks.py"],
    data = [
        ":python/benchmarks/test_data/uncased_L-12_H-768_A-12/vocab.txt",
        ":python/ops/test_data/test_oss_model.model",
        ":test_data_segmenter_hub_module",
    ],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":benchmark_utils",
        ":ops",
        "@absl_py//absl:app",
        "@absl_py//absl/testing:parameterized",
        # python:dtypes tensorflow dep,
        # python/user_ops:ops tensorflow dep,
    ],
)

py_test(
    name = "ops_benchmarks",
    size = "medium",
    srcs = ["python/benchmarks/ops_benchmarks.py"],
    python_version = "PY3",
    srcs_version = "PY3",
    deps = [
        ":benchmark_utils",
        ":ops",
        "@absl_py//absl:app",
        "@absl_py//absl/testing:parameterized",
        # python/user_ops:ops tensorflow dep,
    ],
)
