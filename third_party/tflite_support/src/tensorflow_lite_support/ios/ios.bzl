"""TensorFlow Lite Support Library Helper Rules for iOS"""

# When the static framework is built with bazel, the all header files are moved
# to the "Headers" directory with no header path prefixes. This auxiliary rule
# is used for stripping the path prefix to the C API header files included by
# other C API header files.
def strip_c_api_include_path_prefix(name, hdr_labels, prefix = ""):
    """Create modified header files with the common.h include path stripped out.

    Args:
      name: The name to be used as a prefix to the generated genrules.
      hdr_labels: List of header labels to strip out the include path. Each
          label must end with a colon followed by the header file name.
      prefix: Optional prefix path to prepend to the header inclusion path.
    """

    for hdr_label in hdr_labels:
        hdr_filename = hdr_label.split(":")[-1]
        hdr_basename = hdr_filename.split(".")[0]

        native.genrule(
            name = "{}_{}".format(name, hdr_basename),
            srcs = [hdr_label],
            outs = [hdr_filename],
            cmd = """
            sed 's|#include ".*/\\([^/]\\{{1,\\}}\\.h\\)"|#include "{}\\1"|'\
            "$(location {})"\
            > "$@"
            """.format(prefix, hdr_label),
        )
