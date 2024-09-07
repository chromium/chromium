load("//third-party/bazel:defs.bzl", _crate_repositories = "crate_repositories")

def _crates_vendor_remote_repository_impl(repository_ctx):
    repository_ctx.symlink(repository_ctx.attr.build_file, "BUILD.bazel")

_crates_vendor_remote_repository = repository_rule(
    implementation = _crates_vendor_remote_repository_impl,
    attrs = {
        "build_file": attr.label(mandatory = True),
    },
)

def _crate_repositories_impl(module_ctx):
    _crate_repositories()
    _crates_vendor_remote_repository(
        name = "crates.io",
        build_file = "//third-party/bazel:BUILD.bazel",
    )

crate_repositories = module_extension(
    implementation = _crate_repositories_impl,
)
