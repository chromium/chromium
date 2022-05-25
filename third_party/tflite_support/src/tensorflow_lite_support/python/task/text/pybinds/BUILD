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
        "//tensorflow_lite_support/examples/task/text/desktop:universal_sentence_encoder_qa_op_resolver",
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
        "//tensorflow_lite_support/examples/task/text/desktop:universal_sentence_encoder_qa_op_resolver",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "//tensorflow_lite_support/python/task/processor/proto:search_options_cc_proto",
        "@pybind11",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)
