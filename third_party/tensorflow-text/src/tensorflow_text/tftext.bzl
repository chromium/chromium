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
                "@org_tensorflow//tensorflow:android": [],
                "@org_tensorflow//tensorflow:ios": [],
                "//conditions:default": ["-pthread"],
            }),
            alwayslink = 1,
            deps = cc_op_kernels + tf_deps(),
        )

        native.cc_binary(
            name = binary_name,
            copts = select({
                "@org_tensorflow//tensorflow:android": [],
                "@org_tensorflow//tensorflow:ios": [],
                "//conditions:default": ["-pthread"],
            }),
            linkshared = 1,
            deps = [
                ":" + library_name,
            ] + tf_deps(),
        )

    if srcs:
        native.py_library(
            name = name,
            srcs = srcs,
            srcs_version = "PY2AND3",
            visibility = visibility,
            data = [":" + binary_name],
            deps = deps,
        )


def tf_deps(deps = []):
    """Generate deps for tensorflow that support android.

    Args:
      deps: Dependencies for linux build.
    """
    # These are "random" deps likely needed by each library (http://b/142433427)
    oss_deps = [
        "@com_google_absl//absl/container:inlined_vector",
        "@com_google_absl//absl/functional:function_ref",
        "@com_google_absl//absl/strings",
        "@com_google_absl//absl/strings:cord",
        "@com_google_absl//absl/types:optional",
        "@com_google_absl//absl/types:span",
    ]
    return select({
        "@org_tensorflow//tensorflow:android": [
            "@org_tensorflow//tensorflow/core:portable_tensorflow_lib_lite",
        ],
        "@org_tensorflow//tensorflow:ios": [
            "@org_tensorflow//tensorflow/core:portable_tensorflow_lib_lite",
        ],
        "//conditions:default": [
            "@local_config_tf//:libtensorflow_framework",
            "@local_config_tf//:tf_header_lib",
        ] + deps + oss_deps,
    })

# A rule to build a TensorFlow OpKernel.
#
# Just like cc_library, but adds alwayslink=1 by default.
def tf_text_kernel_library(
        name,
        srcs = [],
        hdrs = [],
        deps = [],
        copts = [],
        alwayslink = 1):
    native.cc_library(
        name = name,
        srcs = srcs,
        hdrs = hdrs,
        deps = deps,
        copts = copts,
        alwayslink = alwayslink)
