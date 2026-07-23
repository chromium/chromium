use std::env;
use std::ffi::OsString;
use std::process::{self, Command};
use std::str;

fn main() {
    println!("cargo:rerun-if-changed=build.rs");

    let rustc = rustc_minor_version().unwrap_or(u32::MAX);

    if rustc >= 80 {
        println!("cargo:rustc-check-cfg=cfg(exhaustive)");
        println!("cargo:rustc-check-cfg=cfg(opt_level, values(\"s\"))");
        println!("cargo:rustc-check-cfg=cfg(zmij_no_select_unpredictable)");
    }

    if rustc < 88 {
        // https://doc.rust-lang.org/std/hint/fn.select_unpredictable.html
        println!("cargo:rustc-cfg=zmij_no_select_unpredictable");
    }

    if let Some(opt_level) = env::var_os("OPT_LEVEL") {
        if opt_level == "s" || opt_level == "z" {
            println!("cargo:rustc-cfg=opt_level=\"s\"");
        }
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
        eprintln!("Environment variable ${key} is not set during execution of build script");
        process::exit(1);
    })
}
