load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")

package(
    default_visibility = [
        "//tensorflow_lite_support:internal",
    ],
    licenses = ["notice"],  # Apache 2.0
)

pybind_extension(
    name = "_pywrap_text_embedder",
    srcs = [
        "_pywrap_text_embedder.cc",
    ],
    module_name = "_pywrap_text_embedder",
    deps = [
        "//tensorflow_lite_support/cc/task/text:text_embedder",
        "//tensorflow_lite_support/python/task/core/pybinds:task_utils",
        "@pybind11",
        "@pybind11_abseil//pybind11_abseil:status_casters",
        "@pybind11_protobuf//pybind11_protobuf:native_proto_caster",
    ],
)
