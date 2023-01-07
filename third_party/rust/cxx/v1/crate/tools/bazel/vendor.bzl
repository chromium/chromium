"""A module defining a repository rule for vendoring the dependencies
of a crate in the current workspace.
"""

load("@rules_rust//rust:defs.bzl", "rust_common")
load("@rules_rust//rust:repositories.bzl", "load_arbitrary_tool")
load("@rules_rust//rust/platform:triple.bzl", "get_host_triple")

def _impl(repository_ctx):
    # Link cxx repository into @third-party.
    lockfile = repository_ctx.path(repository_ctx.attr.lockfile)
    workspace = lockfile.dirname.dirname
    repository_ctx.symlink(workspace, "workspace")

    # Copy third-party/Cargo.lock since those are the crate versions that the
    # BUILD file is written against.
    vendor_lockfile = repository_ctx.path("workspace/third-party/Cargo.lock")
    root_lockfile = repository_ctx.path("workspace/Cargo.lock")
    _copy_file(repository_ctx, src = vendor_lockfile, dst = root_lockfile)

    # Figure out which version of cargo to use.
    if repository_ctx.attr.target_triple:
        target_triple = repository_ctx.attr.target_triple
    else:
        target_triple = get_host_triple(repository_ctx).str

    # Download cargo.
    load_arbitrary_tool(
        ctx = repository_ctx,
        tool_name = "cargo",
        tool_subdirectories = ["cargo"],
        version = repository_ctx.attr.cargo_version,
        iso_date = repository_ctx.attr.cargo_iso_date,
        target_triple = target_triple,
    )

    cmd = ["{}/bin/cargo".format(repository_ctx.path(".")), "vendor", "--versioned-dirs", "third-party/vendor"]
    result = repository_ctx.execute(
        cmd,
        quiet = True,
        working_directory = "workspace",
    )
    _log_cargo_vendor(repository_ctx, result)
    if result.return_code != 0:
        fail("failed to execute `{}`".format(" ".join(cmd)))

    # Copy lockfile back to third-party/Cargo.lock to reflect any modification
    # performed by Cargo.
    _copy_file(repository_ctx, src = root_lockfile, dst = vendor_lockfile)

    # Produce a token for third_party_glob to depend on so that the necessary
    # sequencing is visible to Bazel.
    repository_ctx.file("BUILD", executable = False)
    repository_ctx.file("vendor.bzl", "vendored = True", executable = False)

def _copy_file(repository_ctx, *, src, dst):
    content = repository_ctx.read(src)
    if not dst.exists or content != repository_ctx.read(dst):
        repository_ctx.file(dst, content = content, executable = False)

def _log_cargo_vendor(repository_ctx, result):
    relevant = ""
    for line in result.stderr.splitlines(True):
        if line.strip() and not line.startswith("To use vendored sources,"):
            relevant += line
    if relevant:
        # Render it as command output.
        # If we just use print(), Bazel will cache and repeat the output even
        # when not rerunning the command.
        print = ["echo", relevant]
        repository_ctx.execute(print, quiet = False)

vendor = repository_rule(
    doc = "A rule used to vendor the dependencies of a crate in the current workspace",
    attrs = {
        "cargo_version": attr.string(
            doc = "The version of cargo to use",
        ),
        "cargo_iso_date": attr.string(
            doc = "The date of the tool (or None, if the version is a specific version)",
        ),
        "target_triple": attr.string(
            doc = "The target triple of the cargo binary to download",
        ),
        "lockfile": attr.label(
            doc = "A lockfile providing the set of crates to vendor",
        ),
    },
    local = True,
    implementation = _impl,
)
