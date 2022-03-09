# Placeholder for internal Python strict library compatibility macro.

package(
    default_visibility = ["//tensorflow_lite_support:users"],
    licenses = ["notice"],  # Apache 2.0
)

py_library(
    name = "base_test",
    testonly = 1,
    srcs = ["base_test.py"],
    srcs_version = "PY3",
)

py_library(
    name = "test_util",
    testonly = 1,
    srcs = ["test_util.py"],
    srcs_version = "PY3",
    deps = [
        "@absl_py//absl/flags",
    ],
)
