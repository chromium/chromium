def genrule(cmd, **kwargs):
    # Resolve a distracting inconsistency between Buck and Bazel.
    # Bazel creates the directory for your output file, while Buck expects the
    # cmd to create it.
    #
    # TODO: send this as a PR to Buck, because Bazel's behavior here is better.
    cmd = "mkdir -p `dirname ${OUT}`; " + cmd
    native.genrule(cmd = cmd, **kwargs)
