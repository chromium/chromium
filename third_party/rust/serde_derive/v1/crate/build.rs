use std::env;
use std::process::Command;
use std::str;

// The rustc-cfg strings below are *not* public API. Please let us know by
// opening a GitHub issue if your build environment requires some way to enable
// these cfgs other than by executing our build script.
fn main() {
    let minor = match rustc_minor_version() {
        Some(minor) => minor,
        None => return,
    };

    // Underscore const names stabilized in Rust 1.37:
    // https://blog.rust-lang.org/2019/08/15/Rust-1.37.0.html#using-unnamed-const-items-for-macros
    if minor < 37 {
        println!("cargo:rustc-cfg=no_underscore_consts");
    }

    // The ptr::addr_of! macro stabilized in Rust 1.51:
    // https://blog.rust-lang.org/2021/03/25/Rust-1.51.0.html#stabilized-apis
    if minor < 51 {
        println!("cargo:rustc-cfg=no_ptr_addr_of");
    }
}

fn rustc_minor_version() -> Option<u32> {
    let rustc = env::var_os("RUSTC")?;
    let output = Command::new(rustc).arg("--version").output().ok()?;
    let version = str::from_utf8(&output.stdout).ok()?;
    let mut pieces = version.split('.');
    if pieces.next() != Some("rustc 1") {
        return None;
    }
    pieces.next()?.parse().ok()
}
