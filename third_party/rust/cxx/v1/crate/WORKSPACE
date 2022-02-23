workspace(name = "cxx.rs")

load("@bazel_tools//tools/build_defs/repo:http.bzl", "http_archive")

http_archive(
    name = "rules_rust",
    sha256 = "4d6aa4554eaf5c7bf6da1dd1371b1455b3234676b234a299791635c50c61df91",
    strip_prefix = "rules_rust-238b998f108a099e5a227dbe312526406dda1f2d",
    urls = [
        # Main branch as of 2021-10-01
        "https://github.com/bazelbuild/rules_rust/archive/238b998f108a099e5a227dbe312526406dda1f2d.tar.gz",
    ],
)

load("@rules_rust//rust:repositories.bzl", "rust_repositories")

RUST_VERSION = "1.55.0"

rust_repositories(
    edition = "2018",
    version = RUST_VERSION,
)

load("//tools/bazel:vendor.bzl", "vendor")

vendor(
    name = "third-party",
    lockfile = "//third-party:Cargo.lock",
    cargo_version = RUST_VERSION,
)
