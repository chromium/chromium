load("//tensorflow_lite_support/python/task:build_defs.bzl", "pybind_extension_may_pack_coral")

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

pybind_extension_may_pack_coral(
    name = "_pywrap_text_embedder",
    srcs = [
        "_pywrap_text_embedder.cc",
    ],
    module_name = "_pywrap_text_embedder",
    deps = [
        "//tensorflow_lite_support/cc/task/text:text_embedder",
        "//tensorflow_lite_support/cc/task/text/utils:text_op_resolver",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_abseil//pybind11_abseil:status_casters",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)

pybind_extension_may_pack_coral(
    name = "_pywrap_text_searcher",
    srcs = [
        "_pywrap_text_searcher.cc",
    ],
    module_name = "_pywrap_text_searcher",
    deps = [
        "//tensorflow_lite_support/cc/task/text:text_searcher",
        "//tensorflow_lite_support/cc/task/text/utils:text_op_resolver",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)

pybind_extension_may_pack_coral(
    name = "_pywrap_nl_classifier",
    srcs = [
        "_pywrap_nl_classifier.cc",
    ],
    module_name = "_pywrap_nl_classifier",
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:class_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:classifications_cc_proto",
        "//tensorflow_lite_support/cc/task/text/nlclassifier:nl_classifier",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)

pybind_extension_may_pack_coral(
    name = "_pywrap_bert_nl_classifier",
    srcs = [
        "_pywrap_bert_nl_classifier.cc",
    ],
    module_name = "_pywrap_bert_nl_classifier",
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:class_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:classifications_cc_proto",
        "//tensorflow_lite_support/cc/task/text:bert_nl_classifier",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)

pybind_extension_may_pack_coral(
    name = "_pywrap_bert_question_answerer",
    srcs = [
        "_pywrap_bert_question_answerer.cc",
    ],
    module_name = "_pywrap_bert_question_answerer",
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:qa_answers_cc_proto",
        "//tensorflow_lite_support/cc/task/text:bert_question_answerer",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)

pybind_extension_may_pack_coral(
    name = "_pywrap_bert_clu_annotator",
    srcs = [
        "_pywrap_bert_clu_annotator.cc",
    ],
    module_name = "_pywrap_bert_clu_annotator",
    deps = [
        "//tensorflow_lite_support/cc/task/processor/proto:clu_annotation_options_cc_proto",
        "//tensorflow_lite_support/cc/task/processor/proto:clu_cc_proto",
        "//tensorflow_lite_support/cc/task/text:bert_clu_annotator",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)
