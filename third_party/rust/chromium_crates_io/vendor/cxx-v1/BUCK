load(":Cargo.toml", cargo_toml = "value")

CARGO_PKG_VERSION_PATCH = cargo_toml["package"]["version"].split(".")[2]

rust_library(
    name = "cxx",
    srcs = glob(["src/**/*.rs"]),
    doc_deps = [
        ":cxx-build",
    ],
    edition = "2021",
    features = [
        "alloc",
        "std",
    ],
    visibility = ["PUBLIC"],
    deps = [
        ":core",
        ":cxxbridge-macro",
        "//third-party:foldhash",
    ],
)

alias(
    name = "codegen",
    actual = ":cxxbridge",
    visibility = ["PUBLIC"],
)

rust_binary(
    name = "cxxbridge",
    srcs = glob([
        "gen/cmd/src/**/*.rs",
        "gen/src/builtin/*.h",
    ]) + [
        "gen/cmd/src/gen",
        "gen/cmd/src/syntax",
    ],
    edition = "2021",
    env = {
        "CARGO_PKG_VERSION_PATCH": CARGO_PKG_VERSION_PATCH,
    },
    deps = [
        "//third-party:clap",
        "//third-party:codespan-reporting",
        "//third-party:indexmap",
        "//third-party:proc-macro2",
        "//third-party:quote",
        "//third-party:syn",
    ],
)

cxx_library(
    name = "core",
    srcs = ["src/cxx.cc"],
    exported_headers = {
        "cxx.h": "include/cxx.h",
    },
    header_namespace = "rust",
    preferred_linkage = "static",
    visibility = ["PUBLIC"],
)

rust_library(
    name = "cxxbridge-macro",
    srcs = glob(["macro/src/**/*.rs"]) + ["macro/src/syntax"],
    doctests = False,
    edition = "2021",
    env = {
        "CARGO_PKG_VERSION_PATCH": CARGO_PKG_VERSION_PATCH,
    },
    proc_macro = True,
    deps = [
        "//third-party:indexmap",
        "//third-party:proc-macro2",
        "//third-party:quote",
        "//third-party:rustversion",
        "//third-party:syn",
    ],
)

rust_library(
    name = "cxx-build",
    srcs = glob([
        "gen/build/src/**/*.rs",
        "gen/src/builtin/*.h",
    ]) + [
        "gen/build/src/gen",
        "gen/build/src/syntax",
    ],
    doctests = False,
    edition = "2021",
    env = {
        "CARGO_PKG_VERSION_PATCH": CARGO_PKG_VERSION_PATCH,
    },
    deps = [
        "//third-party:cc",
        "//third-party:codespan-reporting",
        "//third-party:indexmap",
        "//third-party:proc-macro2",
        "//third-party:quote",
        "//third-party:scratch",
        "//third-party:syn",
    ],
)

rust_library(
    name = "cxx-gen",
    srcs = glob([
        "gen/lib/src/**/*.rs",
        "gen/src/builtin/*.h",
    ]) + [
        "gen/lib/src/gen",
        "gen/lib/src/syntax",
    ],
    edition = "2021",
    env = {
        "CARGO_PKG_VERSION_PATCH": CARGO_PKG_VERSION_PATCH,
    },
    visibility = ["PUBLIC"],
    deps = [
        "//third-party:cc",
        "//third-party:codespan-reporting",
        "//third-party:indexmap",
        "//third-party:proc-macro2",
        "//third-party:quote",
        "//third-party:syn",
    ],
)
