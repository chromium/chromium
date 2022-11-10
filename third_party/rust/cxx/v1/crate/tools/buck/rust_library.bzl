load("//tools/buck:genrule.bzl", "genrule")

def rust_library(
        name,
        srcs,
        edition,
        features = [],
        rustc_flags = [],
        build_script = None,
        **kwargs):
    if build_script:
        rust_binary(
            name = "%s@build" % name,
            srcs = srcs + [build_script],
            crate = "build",
            crate_root = build_script,
            edition = edition,
            features = features,
            rustc_flags = rustc_flags,
        )

        genrule(
            name = "%s@cfg" % name,
            out = "output",
            cmd = "env RUSTC=rustc TARGET= $(exe :%s@build) | sed -n s/^cargo:rustc-cfg=/--cfg=/p > ${OUT}" % name,
        )

        rustc_flags = rustc_flags + ["@$(location :%s@cfg)" % name]

    native.rust_library(
        name = name,
        srcs = srcs,
        edition = edition,
        features = features,
        rustc_flags = rustc_flags,
        **kwargs
    )
