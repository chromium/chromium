load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")

package(
    default_applicable_licenses = ["//tensorflow_lite_support:license"],
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

pybind_extension(
    name = "_pywrap_flatbuffers",
    srcs = [
        "flatbuffers_lib.cc",
    ],
    features = ["-use_header_modules"],
    module_name = "_pywrap_flatbuffers",
    deps = [
        "@flatbuffers",
        "@local_config_python//:python_headers",
        "@pybind11",
    ],
)
