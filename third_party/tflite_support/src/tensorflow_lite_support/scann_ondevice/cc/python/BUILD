load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],
)

pybind_extension(
    name = "index_builder",
    srcs = ["index_builder_py_wrapper.cc"],
    module_name = "index_builder",
    deps = [
        "//tensorflow_lite_support/scann_ondevice/cc:index_builder",
        "//tensorflow_lite_support/scann_ondevice/cc/core:serialized_searcher_cc_proto",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
        "@pybind11",
        "@pybind11_abseil//pybind11_abseil:absl_casters",
        "@pybind11_abseil//pybind11_abseil:status_casters",
    ],
)
