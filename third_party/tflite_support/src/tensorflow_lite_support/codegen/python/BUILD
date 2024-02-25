# Placeholder: load py_binary
load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")

package(
    default_visibility = [
        "//visibility:public",
    ],
    licenses = ["notice"],  # Apache 2.0
)

pybind_extension(
    name = "_pywrap_codegen",
    srcs = [
        "codegen_lib.cc",
    ],
    enable_stub_generation = True,
    features = ["-use_header_modules"],
    module_name = "_pywrap_codegen",
    pytype_srcs = [
        "_pywrap_codegen.pyi",
    ],
    deps = [
        "//tensorflow_lite_support/codegen:android_java_generator",
        "//tensorflow_lite_support/codegen:code_generator",
        "@local_config_python//:python_headers",
        "@pybind11",
    ],
)

py_binary(
    name = "codegen",
    srcs = [
        "codegen.py",
    ],
    python_version = "PY3",
    deps = [
        ":_pywrap_codegen",
        "@absl_py//absl:app",
        "@absl_py//absl/flags",
        "@absl_py//absl/logging",
    ],
)
