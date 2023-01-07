"""Build rule to depend on files downloaded from http_file."""

def tflite_file(name, extension):
    """Links the tflite file from http_file with the current directory.

    Args:
      name: the name of the tflite_file target, which is also the name of the
      tflite file specified through http_file in WORKSPACE. For example, if
      `name` is Foo, `tflite_file` will create a link to the downloaded file
      file "@Foo//file" to the current directory as "Foo.tflite".
      extension: the extension of the file.
    """
    native.genrule(
        name = "%s_ln" % (name),
        srcs = ["@%s//file" % (name)],
        outs = ["%s.%s" % (name, extension)],
        output_to_bindir = 1,
        cmd = "ln $< $@",
    )

    native.filegroup(
        name = name,
        srcs = ["%s.%s" % (name, extension)],
    )

def tflite_model(name):
    """Links the tflite model from http_file with the current directory."""
    tflite_file(name, "tflite")
