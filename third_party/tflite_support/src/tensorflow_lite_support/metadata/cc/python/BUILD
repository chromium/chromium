load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")

package(
    default_applicable_licenses = ["//tensorflow_lite_support:license"],
    default_visibility = ["//tensorflow_lite_support:internal"],
    licenses = ["notice"],  # Apache 2.0
)

pybind_extension(
    name = "_pywrap_metadata_version",
    srcs = [
        "metadata_version.cc",
    ],
    features = ["-use_header_modules"],
    module_name = "_pywrap_metadata_version",
    deps = [
        "//tensorflow_lite_support/metadata/cc:metadata_version",
        "@org_tensorflow//tensorflow/lite/c:common",
        "@pybind11",
    ],
)
