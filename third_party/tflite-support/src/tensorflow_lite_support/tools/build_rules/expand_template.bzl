"""Build macro for libzip."""

# forked from kythe/kythe/tools/build_rules/expand_template.bzl
def _expand_template_impl(ctx):
    ctx.actions.expand_template(
        template = ctx.file.template,
        output = ctx.outputs.out,
        substitutions = ctx.attr.substitutions,
    )

expand_template = rule(
    attrs = {
        "out": attr.output(mandatory = True),
        "substitutions": attr.string_dict(mandatory = True),
        "template": attr.label(
            mandatory = True,
            allow_single_file = True,
        ),
    },
    output_to_genfiles = True,
    implementation = _expand_template_impl,
)

def cmake_substitutions(vars, defines = {}):
    """Returns a dict of template substitutions combining `vars` and `defines`.

    Args:
      vars: will be turned into a dict replacing `${key}` and `@key@` with `value`.
      defines: will be turned into a dict replacing `#cmakedefine` with `#define {value}`
        if present is true, otherwise `/* #undef %s /*`.
    Returns:
      substitutions
    """
    subs = {}
    for key, value in vars.items():
        subs["${%s}" % (key,)] = str(value) if value != None else ""
        subs["@%s@" % (key,)] = str(value) if value != None else ""

    # TODO(shahms): Better handling of #cmakedefine delimiters and line endings to
    # avoid the prefix-substitution problem.
    # Potentially allow value to be: True, False, None or string.
    #   True/False => Same as current
    #   None       => assume no suffix value, include \n in sub and replacement
    #   string     => use string to lookup in vars and assume ${} or @@ tail?
    for macro, present in defines.items():
        if present:
            subs["#cmakedefine %s" % macro] = "#define %s" % macro
        else:
            subs["#cmakedefine %s" % macro] = "/* #undef %s */" % macro
    return subs
