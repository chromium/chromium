// rustc-cfg emitted by the build script:
//
// "wrap_proc_macro"
//     Wrap types from libproc_macro rather than polyfilling the whole API.
//     Enabled on rustc 1.29+ as long as procmacro2_semver_exempt is not set,
//     because we can't emulate the unstable API without emulating everything
//     else. Also enabled unconditionally on nightly, in which case the
//     procmacro2_semver_exempt surface area is implemented by using the
//     nightly-only proc_macro API.
//
// "hygiene"
//    Enable Span::mixed_site() and non-dummy behavior of Span::resolved_at
//    and Span::located_at. Enabled on Rust 1.45+.
//
// "proc_macro_span"
//     Enable non-dummy behavior of Span::start and Span::end methods which
//     requires an unstable compiler feature. Enabled when building with
//     nightly, unless `-Z allow-feature` in RUSTFLAGS disallows unstable
//     features.
//
// "super_unstable"
//     Implement the semver exempt API in terms of the nightly-only proc_macro
//     API. Enabled when using procmacro2_semver_exempt on a nightly compiler.
//
// "span_locations"
//     Provide methods Span::start and Span::end which give the line/column
//     location of a token. Enabled by procmacro2_semver_exempt or the
//     "span-locations" Cargo cfg. This is behind a cfg because tracking
//     location inside spans is a performance hit.
//
// "is_available"
//     Use proc_macro::is_available() to detect if the proc macro API is
//     available or needs to be polyfilled instead of trying to use the proc
//     macro API and catching a panic if it isn't available. Enabled on Rust
//     1.57+.

use std::env;
use std::ffi::OsString;
use std::path::Path;
use std::process::{self, Command, Stdio};
use std::str;
use std::u32;

fn main() {
    let rustc = rustc_minor_version().unwrap_or(u32::MAX);

    let docs_rs = env::var_os("DOCS_RS").is_some();
    let semver_exempt = cfg!(procmacro2_semver_exempt) || docs_rs;
    if semver_exempt {
        // https://github.com/dtolnay/proc-macro2/issues/147
        println!("cargo:rustc-cfg=procmacro2_semver_exempt");
    }

    if semver_exempt || cfg!(feature = "span-locations") {
        println!("cargo:rustc-cfg=span_locations");
    }

    if rustc < 57 {
        println!("cargo:rustc-cfg=no_is_available");
    }

    if rustc < 66 {
        println!("cargo:rustc-cfg=no_source_text");
    }

    if !cfg!(feature = "proc-macro") {
        println!("cargo:rerun-if-changed=build.rs");
        return;
    }

    println!("cargo:rerun-if-changed=build/probe.rs");

    let proc_macro_span;
    let consider_rustc_bootstrap;
    if compile_probe(false) {
        // This is a nightly or dev compiler, so it supports unstable features
        // regardless of RUSTC_BOOTSTRAP. No need to rerun build script if
        // RUSTC_BOOTSTRAP is changed.
        proc_macro_span = true;
        consider_rustc_bootstrap = false;
    } else if let Some(rustc_bootstrap) = env::var_os("RUSTC_BOOTSTRAP") {
        if compile_probe(true) {
            // This is a stable or beta compiler for which the user has set
            // RUSTC_BOOTSTRAP to turn on unstable features. Rerun build script
            // if they change it.
            proc_macro_span = true;
            consider_rustc_bootstrap = true;
        } else if rustc_bootstrap == "1" {
            // This compiler does not support the proc macro Span API in the
            // form that proc-macro2 expects. No need to pay attention to
            // RUSTC_BOOTSTRAP.
            proc_macro_span = false;
            consider_rustc_bootstrap = false;
        } else {
            // This is a stable or beta compiler for which RUSTC_BOOTSTRAP is
            // set to restrict the use of unstable features by this crate.
            proc_macro_span = false;
            consider_rustc_bootstrap = true;
        }
    } else {
        // Without RUSTC_BOOTSTRAP, this compiler does not support the proc
        // macro Span API in the form that proc-macro2 expects, but try again if
        // the user turns on unstable features.
        proc_macro_span = false;
        consider_rustc_bootstrap = true;
    }

    if proc_macro_span || !semver_exempt {
        println!("cargo:rustc-cfg=wrap_proc_macro");
    }

    if proc_macro_span {
        println!("cargo:rustc-cfg=proc_macro_span");
    }

    if semver_exempt && proc_macro_span {
        println!("cargo:rustc-cfg=super_unstable");
    }

    if consider_rustc_bootstrap {
        println!("cargo:rerun-if-env-changed=RUSTC_BOOTSTRAP");
    }
}

fn compile_probe(rustc_bootstrap: bool) -> bool {
    if env::var_os("RUSTC_STAGE").is_some() {
        // We are running inside rustc bootstrap. This is a highly non-standard
        // environment with issues such as:
        //
        //     https://github.com/rust-lang/cargo/issues/11138
        //     https://github.com/rust-lang/rust/issues/114839
        //
        // Let's just not use nightly features here.
        return false;
    }

    let rustc = cargo_env_var("RUSTC");
    let out_dir = cargo_env_var("OUT_DIR");
    let probefile = Path::new("build").join("probe.rs");

    // Make sure to pick up Cargo rustc configuration.
    let mut cmd = if let Some(wrapper) = env::var_os("RUSTC_WRAPPER") {
        let mut cmd = Command::new(wrapper);
        // The wrapper's first argument is supposed to be the path to rustc.
        cmd.arg(rustc);
        cmd
    } else {
        Command::new(rustc)
    };

    if !rustc_bootstrap {
        cmd.env_remove("RUSTC_BOOTSTRAP");
    }

    cmd.stderr(Stdio::null())
        .arg("--edition=2021")
        .arg("--crate-name=proc_macro2")
        .arg("--crate-type=lib")
        .arg("--emit=dep-info,metadata")
        .arg("--out-dir")
        .arg(out_dir)
        .arg(probefile);

    if let Some(target) = env::var_os("TARGET") {
        cmd.arg("--target").arg(target);
    }

    // If Cargo wants to set RUSTFLAGS, use that.
    if let Ok(rustflags) = env::var("CARGO_ENCODED_RUSTFLAGS") {
        if !rustflags.is_empty() {
            for arg in rustflags.split('\x1f') {
                cmd.arg(arg);
            }
        }
    }

    match cmd.status() {
        Ok(status) => status.success(),
        Err(_) => false,
    }
}

fn rustc_minor_version() -> Option<u32> {
    let rustc = cargo_env_var("RUSTC");
    let output = Command::new(rustc).arg("--version").output().ok()?;
    let version = str::from_utf8(&output.stdout).ok()?;
    let mut pieces = version.split('.');
    if pieces.next() != Some("rustc 1") {
        return None;
    }
    pieces.next()?.parse().ok()
}

fn cargo_env_var(key: &str) -> OsString {
    env::var_os(key).unwrap_or_else(|| {
        eprintln!(
            "Environment variable ${} is not set during execution of build script",
            key,
        );
        process::exit(1);
    })
}
