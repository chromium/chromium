load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")

package(
    default_applicable_licenses = ["//tensorflow_lite_support:license"],
    licenses = ["notice"],
)

pybind_extension(
    name = "leveldb_testing_utils",
    srcs = ["leveldb_testing_utils_py_wrapper.cc"],
    module_name = "leveldb_testing_utils",
    visibility = [
        "//nlp/sage/learning/asqp:__subpackages__",
        "//tensorflow_lite_support:internal",
    ],
    deps = [
        "@com_google_absl//absl/memory",
        "@com_google_absl//absl/status",
        "@com_google_absl//absl/status:statusor",
        "@com_google_absl//absl/strings:str_format",
        "@com_google_leveldb//:table",
        "@com_google_leveldb//:util",
        "@pybind11",
        "@pybind11_abseil//pybind11_abseil:absl_casters",
        "@pybind11_abseil//pybind11_abseil:status_casters",
    ],
)
