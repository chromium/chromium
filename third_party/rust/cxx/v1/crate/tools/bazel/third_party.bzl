load("@rules_rust//cargo:cargo_build_script.bzl", "cargo_build_script")
load("@rules_rust//rust:defs.bzl", "rust_binary", "rust_library")
load("@third-party//:vendor.bzl", "vendored")

def third_party_glob(include):
    return vendored and native.glob(include)

def third_party_cargo_build_script(rustc_flags = [], **kwargs):
    rustc_flags = rustc_flags + ["--cap-lints=allow"]
    cargo_build_script(rustc_flags = rustc_flags, **kwargs)

def third_party_rust_binary(rustc_flags = [], **kwargs):
    rustc_flags = rustc_flags + ["--cap-lints=allow"]
    rust_binary(rustc_flags = rustc_flags, **kwargs)

def third_party_rust_library(rustc_flags = [], **kwargs):
    rustc_flags = rustc_flags + ["--cap-lints=allow"]
    rust_library(rustc_flags = rustc_flags, **kwargs)
