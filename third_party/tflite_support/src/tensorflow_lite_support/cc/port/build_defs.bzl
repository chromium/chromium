""".bzl file for TFLite Support open source build configs."""

def provided_args(**kwargs):
    """Returns the keyword arguments omitting None arguments."""
    return {k: v for k, v in kwargs.items() if v != None}

def support_cc_proto_library(name, deps = [], visibility = None):
    """Generates cc_proto_library for TFLite Support open source version.

      Args:
        name: the name of the cc_proto_library.
        deps: a list of dependency labels for Bazel use; must be proto_library.
        visibility: visibility of this target.
    """

    # Verified in the external path.
    # buildifier: disable=native-cc-proto
    native.cc_proto_library(**provided_args(
        name = name,
        visibility = visibility,
        deps = deps,
    ))
