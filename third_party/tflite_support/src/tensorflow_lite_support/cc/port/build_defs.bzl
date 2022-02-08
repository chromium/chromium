""".bzl file for TFLite Support open source build configs."""

load("@com_google_protobuf//:protobuf.bzl", "py_proto_library")

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

def support_py_proto_library(
        name,
        srcs,
        visibility = None,
        py_proto_deps = [],
        proto_deps = None,
        api_version = None,
        testonly = 0):
    """Generates py_proto_library for TFLite Support open source version.

    Args:
      name: the name of the py_proto_library.
      api_version: api version for internal use only.
      srcs: the .proto files of the py_proto_library for Bazel use.
      visibility: visibility of this target.
      py_proto_deps: a list of dependency labels for Bazel use; must be py_proto_library.
      proto_deps: a list of dependency labels for internal use.
      testonly: test only proto or not.
    """
    _ignore = [api_version, proto_deps]
    py_proto_library(**provided_args(
        name = name,
        srcs = srcs,
        visibility = visibility,
        default_runtime = "@com_google_protobuf//:protobuf_python",
        protoc = "@com_google_protobuf//:protoc",
        deps = py_proto_deps + ["@com_google_protobuf//:protobuf_python"],
        testonly = testonly,
    ))
