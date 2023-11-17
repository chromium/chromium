#![allow(clippy::option_if_let_else)]

use std::env;
use std::fs;
use std::path::Path;
use std::process::{Command, ExitStatus, Stdio};
use std::str;

#[cfg(all(feature = "backtrace", not(feature = "std")))]
compile_error! {
    "`backtrace` feature without `std` feature is not supported"
}

// This code exercises the surface area that we expect of the Error generic
// member access API. If the current toolchain is able to compile it, then
// anyhow is able to provide backtrace support.
const PROBE: &str = r#"
    #![feature(error_generic_member_access)]

    use std::backtrace::Backtrace;
    use std::error::{self, Error, Request};
    use std::fmt::{self, Debug, Display};

    struct MyError(Thing);
    struct Thing;

    impl Debug for MyError {
        fn fmt(&self, _formatter: &mut fmt::Formatter) -> fmt::Result {
            unimplemented!()
        }
    }

    impl Display for MyError {
        fn fmt(&self, _formatter: &mut fmt::Formatter) -> fmt::Result {
            unimplemented!()
        }
    }

    impl Error for MyError {
        fn provide<'a>(&'a self, request: &mut Request<'a>) {
            request.provide_ref(&self.0);
        }
    }

    const _: fn(&dyn Error) -> Option<&Backtrace> = |err| error::request_ref::<Backtrace>(err);
"#;

fn main() {
    if cfg!(feature = "std") {
        match compile_probe() {
            Some(status) if status.success() => println!("cargo:rustc-cfg=backtrace"),
            _ => {}
        }
    }

    let rustc = match rustc_minor_version() {
        Some(rustc) => rustc,
        None => return,
    };

    if rustc < 51 {
        println!("cargo:rustc-cfg=anyhow_no_ptr_addr_of");
    }

    if rustc < 52 {
        println!("cargo:rustc-cfg=anyhow_no_fmt_arguments_as_str");
    }
}

fn compile_probe() -> Option<ExitStatus> {
    if env::var_os("RUSTC_STAGE").is_some() {
        // We are running inside rustc bootstrap. This is a highly non-standard
        // environment with issues such as:
        //
        //     https://github.com/rust-lang/cargo/issues/11138
        //     https://github.com/rust-lang/rust/issues/114839
        //
        // Let's just not use nightly features here.
        return None;
    }

    let rustc = env::var_os("RUSTC")?;
    let out_dir = env::var_os("OUT_DIR")?;
    let probefile = Path::new(&out_dir).join("probe.rs");
    fs::write(&probefile, PROBE).ok()?;

    // Make sure to pick up Cargo rustc configuration.
    let mut cmd = if let Some(wrapper) = env::var_os("RUSTC_WRAPPER") {
        let mut cmd = Command::new(wrapper);
        // The wrapper's first argument is supposed to be the path to rustc.
        cmd.arg(rustc);
        cmd
    } else {
        Command::new(rustc)
    };

    cmd.stderr(Stdio::null())
        .arg("--edition=2018")
        .arg("--crate-name=anyhow_build")
        .arg("--crate-type=lib")
        .arg("--emit=metadata")
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

    cmd.status().ok()
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
