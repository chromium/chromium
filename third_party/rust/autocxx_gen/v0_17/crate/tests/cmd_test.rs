// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use std::{convert::TryInto, fs::File, io::Write, path::Path};

use assert_cmd::Command;
use tempdir::TempDir;

static MAIN_RS: &str = include_str!("../../../demo/src/main.rs");
static INPUT_H: &str = include_str!("../../../demo/src/input.h");
static BLANK: &str = "// Blank autocxx placeholder";

#[test]
fn test_help() -> Result<(), Box<dyn std::error::Error>> {
    let mut cmd = Command::cargo_bin("autocxx-gen")?;
    cmd.arg("-h").assert().success();
    Ok(())
}

fn base_test<F>(tmp_dir: &TempDir, arg_modifier: F) -> Result<(), Box<dyn std::error::Error>>
where
    F: FnOnce(&mut Command),
{
    let result = base_test_ex(tmp_dir, arg_modifier, false);
    assert_contentful(tmp_dir, "gen0.cc");
    result
}

fn base_test_ex<F>(
    tmp_dir: &TempDir,
    arg_modifier: F,
    use_complete_rs: bool,
) -> Result<(), Box<dyn std::error::Error>>
where
    F: FnOnce(&mut Command),
{
    let demo_code_dir = tmp_dir.path().join("demo");
    std::fs::create_dir(&demo_code_dir).unwrap();
    write_to_file(&demo_code_dir, "input.h", INPUT_H.as_bytes());
    write_to_file(&demo_code_dir, "main.rs", MAIN_RS.as_bytes());
    let demo_rs = demo_code_dir.join("main.rs");
    let mut cmd = Command::cargo_bin("autocxx-gen")?;
    arg_modifier(&mut cmd);
    cmd.arg("--inc")
        .arg(demo_code_dir.to_str().unwrap())
        .arg(demo_rs)
        .arg("--outdir")
        .arg(tmp_dir.path().to_str().unwrap())
        .arg("--gen-cpp");
    if use_complete_rs {
        cmd.arg("--gen-rs-complete");
    } else {
        cmd.arg("--gen-rs-include");
    }
    cmd.assert().success();
    Ok(())
}

#[test]
fn test_gen() -> Result<(), Box<dyn std::error::Error>> {
    let tmp_dir = TempDir::new("example")?;
    base_test(&tmp_dir, |_| {})
}

#[test]
fn test_include_prefixes() -> Result<(), Box<dyn std::error::Error>> {
    let tmp_dir = TempDir::new("example")?;
    base_test(&tmp_dir, |cmd| {
        cmd.arg("--cxx-h-path")
            .arg("foo/")
            .arg("--cxxgen-h-path")
            .arg("bar/")
            .arg("--generate-exact")
            .arg("3");
    })?;
    assert_contains(&tmp_dir, "gen0.h", "foo/cxx.h");
    // Currently we don't test cxxgen-h-path because we build the demo code
    // which doesn't refer to generated cxx header code.
    Ok(())
}

#[test]
fn test_gen_fixed_num() -> Result<(), Box<dyn std::error::Error>> {
    let tmp_dir = TempDir::new("example")?;
    base_test_ex(
        &tmp_dir,
        |cmd| {
            cmd.arg("--generate-exact").arg("3");
        },
        true,
    )?;
    assert_contentful(&tmp_dir, "gen0.cc");
    assert_contentful(&tmp_dir, "gen0.h");
    // TODO: This file will actually try #including one of our .h files if there's anything to put
    // in it. Figure out how to make that happen to test that it works.
    assert_not_contentful(&tmp_dir, "gen1.cc");
    assert_not_contentful(&tmp_dir, "gen1.h");
    assert_not_contentful(&tmp_dir, "gen2.cc");
    assert_not_contentful(&tmp_dir, "gen2.h");
    assert_contentful(&tmp_dir, "cxxgen.h");
    assert_contentful(&tmp_dir, "gen.complete.rs");
    Ok(())
}

#[test]
fn test_gen_preprocess() -> Result<(), Box<dyn std::error::Error>> {
    let tmp_dir = TempDir::new("example")?;
    let prepro_path = tmp_dir.path().join("preprocessed.h");
    base_test(&tmp_dir, |cmd| {
        cmd.env("AUTOCXX_PREPROCESS", prepro_path.to_str().unwrap());
    })?;
    assert_contentful(&tmp_dir, "preprocessed.h");
    // Check that a random thing from one of the headers in
    // `ALL_KNOWN_SYSTEM_HEADERS` is included.
    assert!(std::fs::read_to_string(prepro_path)?.contains("integer_sequence"));
    Ok(())
}

#[test]
fn test_gen_repro() -> Result<(), Box<dyn std::error::Error>> {
    let tmp_dir = TempDir::new("example")?;
    let repro_path = tmp_dir.path().join("repro.json");
    base_test(&tmp_dir, |cmd| {
        cmd.env("AUTOCXX_REPRO_CASE", repro_path.to_str().unwrap());
    })?;
    assert_contentful(&tmp_dir, "repro.json");
    // Check that a random thing from one of the headers in
    // `ALL_KNOWN_SYSTEM_HEADERS` is included.
    assert!(std::fs::read_to_string(repro_path)?.contains("integer_sequence"));
    Ok(())
}

#[test]
fn test_skip_cxx_gen() -> Result<(), Box<dyn std::error::Error>> {
    let tmp_dir = TempDir::new("example")?;
    base_test_ex(
        &tmp_dir,
        |cmd| {
            cmd.arg("--generate-exact")
                .arg("3")
                .arg("--fix-rs-include-name")
                .arg("--skip-cxx-gen");
        },
        false,
    )?;
    assert_contentful(&tmp_dir, "gen0.h");
    assert_not_contentful(&tmp_dir, "gen0.cc");
    assert_exists(&tmp_dir, "gen1.cc");
    assert_exists(&tmp_dir, "gen1.h");
    assert_exists(&tmp_dir, "gen2.cc");
    assert_exists(&tmp_dir, "gen2.h");
    assert_contentful(&tmp_dir, "gen0.include.rs");
    assert_exists(&tmp_dir, "gen1.include.rs");
    assert_exists(&tmp_dir, "gen2.include.rs");
    Ok(())
}

fn write_to_file(dir: &Path, filename: &str, content: &[u8]) {
    let path = dir.join(filename);
    let mut f = File::create(&path).expect("Unable to create file");
    f.write_all(content).expect("Unable to write file");
}

fn assert_contentful(outdir: &TempDir, fname: &str) {
    let p = outdir.path().join(fname);
    if !p.exists() {
        panic!("File {} didn't exist", p.to_string_lossy());
    }
    assert!(
        p.metadata().unwrap().len() > BLANK.len().try_into().unwrap(),
        "File {} is empty",
        fname
    );
}

fn assert_not_contentful(outdir: &TempDir, fname: &str) {
    let p = outdir.path().join(fname);
    if !p.exists() {
        panic!("File {} didn't exist", p.to_string_lossy());
    }
    assert!(
        p.metadata().unwrap().len() <= BLANK.len().try_into().unwrap(),
        "File {} is not empty",
        fname
    );
}

fn assert_exists(outdir: &TempDir, fname: &str) {
    let p = outdir.path().join(fname);
    if !p.exists() {
        panic!("File {} didn't exist", p.to_string_lossy());
    }
}

fn assert_contains(outdir: &TempDir, fname: &str, pattern: &str) {
    let p = outdir.path().join(fname);
    let content = std::fs::read_to_string(&p).expect(fname);
    eprintln!("content = {}", content);
    assert!(content.contains(pattern));
}
