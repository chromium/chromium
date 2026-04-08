// Copyright 2026 The Fuchsia Authors
//
// Licensed under a BSD-style license <LICENSE-BSD>, Apache License, Version 2.0
// <LICENSE-APACHE or https://www.apache.org/licenses/LICENSE-2.0>, or the MIT
// license <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your option.
// This file may not be copied, modified, or distributed except according to
// those terms.

#![cfg(__ZEROCOPY_INTERNAL_USE_ONLY_NIGHTLY_FEATURES_IN_TESTS)]

use std::{panic, path::PathBuf, process::Command, thread};

use regex::Regex;

enum Directive {
    Asm,
    Mca,
}

impl Directive {
    fn arg(&self) -> &'static str {
        match self {
            Directive::Asm => "--asm",
            Directive::Mca => "--mca",
        }
    }

    fn ext(&self) -> &'static str {
        match self {
            Directive::Asm => "",
            Directive::Mca => ".mca",
        }
    }
}

fn run_codegen_test(bench_name: &str, target_cpu: &str, bless: bool) {
    println!("Testing {bench_name}.{target_cpu}");

    let manifest_path = env!("CARGO_MANIFEST_PATH");
    let target_dir = env!("CARGO_TARGET_DIR");

    let cargo_asm = |directive: &Directive| {
        Command::new("./cargo.sh")
            .args([
                "+nightly",
                "asm",
                "--quiet",
                "-p",
                "zerocopy",
                "--manifest-path",
                manifest_path,
                "--target-dir",
                target_dir,
                "--all-features",
                "--bench",
                bench_name,
                "--target-cpu",
                target_cpu,
                "--simplify",
                directive.arg(),
                &format!("bench_{bench_name}"),
            ])
            .output()
            .expect("failed to execute process")
    };

    let re = Regex::new(r"(\.Lanon\.)[0-z]+(\.\d+)").unwrap();

    let test_directive = |directive: Directive| {
        let output = cargo_asm(&directive);
        let actual_result = String::from_utf8_lossy(&output.stdout);
        let actual_result = re.replace_all(&actual_result, "${1}HASH${2}");

        if !(output.status.success()) {
            panic!("{}\n{}", &actual_result, String::from_utf8_lossy(&output.stderr));
        }

        let expected_file_path = {
            let ext = directive.ext();
            let mut path: PathBuf = env!("CARGO_MANIFEST_DIR").into();
            path.push("benches");
            let file_name = format!("{bench_name}.{target_cpu}{ext}",);
            path.push(file_name);
            path
        };

        if bless {
            std::fs::write(expected_file_path, actual_result.as_bytes()).unwrap();
        } else {
            let expected_result = std::fs::read(expected_file_path).unwrap_or_default();
            if actual_result.as_bytes() != expected_result {
                let expected = String::from_utf8_lossy(&expected_result[..]);
                panic!("Bless codegen tests with BLESS=1\nGot unexpected output:\n{}", expected);
            }
        }
    };

    test_directive(Directive::Asm);
    test_directive(Directive::Mca);
}

#[test]
#[cfg_attr(miri, ignore)]
fn codegen() {
    let bless = std::env::var("BLESS").is_ok();
    let handles: Vec<_> = std::fs::read_dir("benches")
        .unwrap()
        .map(|entry| entry.unwrap().path())
        .filter(|path| path.extension().is_some_and(|ext| ext == "rs"))
        .map(|path| {
            let bench_name = path.file_stem().unwrap().to_str().unwrap().to_owned();
            thread::spawn(move || {
                panic::catch_unwind(panic::AssertUnwindSafe(|| {
                    run_codegen_test(&bench_name, "x86-64", bless);
                }))
            })
        })
        .collect();

    let failed = handles.into_iter().any(|handle| handle.join().unwrap().is_err());

    if failed {
        panic!("One or more codegen tests failed. See thread panics above for details.");
    }
}
