workspace(name = "cxx.rs")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_rust",
    sha256 = "617082067629939c0a22f587811a3e822a50a203119a90380e21f5aec3373da9",
    strip_prefix = "rules_rust-e07881fa22a5f0d16230d8b23bbff2bf358823b8",
    urls = [
        # Main branch as of 2022-04-27
        "https://github.com/bazelbuild/rules_rust/archive/e07881fa22a5f0d16230d8b23bbff2bf358823b8.tar.gz",
    ],
)

load("@rules_rust//rust:repositories.bzl", "rules_rust_dependencies", "rust_register_toolchains")

RUST_VERSION = "1.62.0"

rules_rust_dependencies()

rust_register_toolchains(
    version = RUST_VERSION,
)

load("//tools/bazel:vendor.bzl", "vendor")

vendor(
    name = "third-party",
    cargo_version = RUST_VERSION,
    lockfile = "//third-party:Cargo.lock",
)
