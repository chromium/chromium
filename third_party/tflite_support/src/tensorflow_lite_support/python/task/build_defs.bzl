""".bzl file for Acceleration allowlisting."""

load("@org_tensorflow//tensorflow:tensorflow.bzl", "pybind_extension")

def pybind_extension_may_pack_coral(name, deps, **kwargs):
    """Defines a pybind_extension rule that optionally depends on Coral.

    It pulls in Coral EdgeTPU plugin dependency when passing
    `--define darwinn_portable=1` to the build command.

    Args:
      name: determines the name used for the generated pybind_extension target.
      deps: dependencies that will be unconditionally included in the deps of
        the generated pybind_extension targets.
      **kwargs:
        Additional pybind_extension parameters.
    """
    pybind_extension(
        name = name,
        # Note that `darwinn_portable` is used not only when selecting
        # `edgetpu_coral_plugin` here, but also a necessary flag to build
        # `edgetpu_coral_plugin`.
        deps = deps + select({
            "//tensorflow_lite_support/examples/task:darwinn_portable": [
                "//tensorflow_lite_support/acceleration/configuration:edgetpu_coral_plugin",
            ],
            "//conditions:default": [
            ],
        }),
        **kwargs
    )
