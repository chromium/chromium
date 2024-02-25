load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")

package(
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

pybind_extension(
    name = "_pywrap_flatbuffers",
    srcs = [
        "flatbuffers_lib.cc",
    ],
    enable_stub_generation = True,
    features = ["-use_header_modules"],
    module_name = "_pywrap_flatbuffers",
    pytype_srcs = [
        "_pywrap_flatbuffers.pyi",
    ],
    deps = [
        "@flatbuffers",
        "@local_config_python//:python_headers",
        "@pybind11",
    ],
)
