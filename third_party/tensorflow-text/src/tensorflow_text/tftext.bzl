"""
Build rule for open source tf.text libraries.
"""

def py_tf_text_library(
        name,
        srcs = [],
        deps = [],
        visibility = None,
        cc_op_defs = [],
        cc_op_kernels = []):
    """Creates build rules for TF.Text ops as shared libraries.

    Defines three targets:

    <name>
        Python library that exposes all ops defined in `cc_op_defs` and `py_srcs`.
    <name>_cc
        C++ library that registers any c++ ops in `cc_op_defs`, and includes the
        kernels from `cc_op_kernels`.
    python/ops/_<name>.so
        Shared library exposing the <name>_cc library.

    Args:
      name: The name for the python library target build by this rule.
      srcs: Python source files for the Python library.
      deps: Dependencies for the Python library.
      visibility: Visibility for the Python library.
      cc_op_defs: A list of c++ src files containing REGISTER_OP definitions.
      cc_op_kernels: A list of c++ targets containing kernels that are used
          by the Python library.
    """
    binary_path = "python/ops"
    if srcs:
        binary_path_end_pos = srcs[0].rfind("/")
        binary_path = srcs[0][0:binary_path_end_pos]
    binary_name = binary_path + "/_" + cc_op_kernels[0][1:] + ".so"
    if cc_op_defs:
        binary_name = binary_path + "/_" + name + ".so"
        library_name = name + "_cc"
        native.cc_library(
            name = library_name,
            srcs = cc_op_defs,
            copts = select({
                # Android supports pthread natively, -pthread is not needed.
                "@org_tensorflow//tensorflow:mobile": [],
                "//conditions:default": ["-pthread"],
            }),
            alwayslink = 1,
            deps = cc_op_kernels + select({
                "@org_tensorflow//tensorflow:mobile": [
                    "@org_tensorflow//tensorflow/core:portable_tensorflow_lib_lite",
                ],
                "//conditions:default": [],
            }),
        )

        native.cc_binary(
            name = binary_name,
            copts = select({
                "@org_tensorflow//tensorflow:mobile": [],
                "//conditions:default": ["-pthread"],
            }),
            linkshared = 1,
            deps = [
                ":" + library_name,
            ] + select({
                "@org_tensorflow//tensorflow:mobile": [
                    "@org_tensorflow//tensorflow/core:portable_tensorflow_lib_lite",
                ],
                "//conditions:default": [],
            }),
        )

    native.py_library(
        name = name,
        srcs = srcs,
        srcs_version = "PY2AND3",
        visibility = visibility,
        data = [":" + binary_name],
        deps = deps,
    )


def tf_cc_library(
        name,
        srcs = [],
        hdrs = [],
        deps = [],
        tf_deps = [],
        copts = [],
        compatible_with = None,
        testonly = 0,
        alwayslink = 0):
    """ A rule to build a TensorFlow library or OpKernel.

    Just like cc_library, but:
      * Adds alwayslink=1 for kernels (name has kernel in it)
      * Separates out TF deps for when building for Android.

    Args:
        name: Name of library
        srcs: Source files
        hdrs: Headers files
        deps: All non-TF dependencies
        tf_deps: All TF depenedencies
        copts: C options
        compatible_with: List of environments target can be built for
        testonly: If library is only for testing
        alwayslink: If symbols should be exported
    """
    if "kernel" in name:
        alwayslink = 1
    # These are "random" deps likely needed by each library (http://b/142433427)
    oss_deps = [
        "@com_google_absl//absl/container:flat_hash_map",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/time",
        "@com_google_absl//absl/types:variant",
    ]
    deps += select({
        "@org_tensorflow//tensorflow:mobile": [
            "@org_tensorflow//tensorflow/core:portable_tensorflow_lib_lite",
        ],
        "//conditions:default": [
            "@local_config_tf//:libtensorflow_framework",
            "@local_config_tf//:tf_header_lib",
        ] + tf_deps + oss_deps,
    })
    native.cc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        deps = deps,
        copts = copts,
        compatible_with = compatible_with,
        testonly = testonly,
        alwayslink = alwayslink)
