#![allow(unknown_lints)]
#![allow(unexpected_cfgs)]

use std::env;
use std::path::{Path, PathBuf};
use std::process::Command;

fn main() {
    let manifest_dir_opt = env::var_os("CARGO_MANIFEST_DIR").map(PathBuf::from);
    let manifest_dir = manifest_dir_opt.as_deref().unwrap_or(Path::new(""));

    cc::Build::new()
        .file(manifest_dir.join("src/cxx.cc"))
        .cpp(true)
        .cpp_link_stdlib(None) // linked via link-cplusplus crate
        .std(cxxbridge_flags::STD)
        .warnings_into_errors(cfg!(deny_warnings))
        .compile("cxxbridge1");

    println!("cargo:rerun-if-changed=src/cxx.cc");
    println!("cargo:rerun-if-changed=include/cxx.h");
    println!("cargo:rustc-cfg=built_with_cargo");

    if let Some(manifest_dir) = &manifest_dir_opt {
        let cxx_h = manifest_dir.join("include").join("cxx.h");
        println!("cargo:HEADER={}", cxx_h.to_string_lossy());
    }

    if let Some(rustc) = rustc_version() {
        if rustc.minor >= 80 {
            println!("cargo:rustc-check-cfg=cfg(built_with_cargo)");
            println!("cargo:rustc-check-cfg=cfg(compile_error_if_alloc)");
            println!("cargo:rustc-check-cfg=cfg(compile_error_if_std)");
            println!("cargo:rustc-check-cfg=cfg(cxx_experimental_no_alloc)");
            println!("cargo:rustc-check-cfg=cfg(error_in_core)");
            println!("cargo:rustc-check-cfg=cfg(skip_ui_tests)");
        }

        if rustc.minor < 67 {
            println!("cargo:warning=The cxx crate requires a rustc version 1.67.0 or newer.");
            println!(
                "cargo:warning=You appear to be building with: {}",
                rustc.version,
            );
        }

        if rustc.minor >= 81 {
            // core::error::Error
            println!("cargo:rustc-cfg=error_in_core");
        }
    }
}

struct RustVersion {
    version: String,
    minor: u32,
}

fn rustc_version() -> Option<RustVersion> {
    let rustc = env::var_os("RUSTC")?;
    let output = Command::new(rustc).arg("--version").output().ok()?;
    let version = String::from_utf8(output.stdout).ok()?;
    let mut pieces = version.split('.');
    if pieces.next() != Some("rustc 1") {
        return None;
    }
    let minor = pieces.next()?.parse().ok()?;
    Some(RustVersion { version, minor })
}
