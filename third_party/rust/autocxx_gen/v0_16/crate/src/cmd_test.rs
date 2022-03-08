// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//    https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

use std::{convert::TryInto, fs::File, io::Write, path::Path};

use assert_cmd::Command;
use tempdir::TempDir;

static MAIN_RS: &str = include_str!("../../../demo/src/main.rs");
static INPUT_H: &str = include_str!("../../../demo/src/input.h");

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
        .arg("--gen-cpp")
        .arg("--gen-rs-include")
        .assert()
        .success();
    assert_contentful(&tmp_dir, "gen0.cc");
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
    assert_contains(&tmp_dir, "autocxxgen_ffi.h", "foo/cxx.h");
    // Currently we don't test cxxgen-h-path because we build the demo code
    // which doesn't refer to generated cxx header code.
    Ok(())
}

#[test]
fn test_gen_fixed_num() -> Result<(), Box<dyn std::error::Error>> {
    let tmp_dir = TempDir::new("example")?;
    base_test(&tmp_dir, |cmd| {
        cmd.arg("--generate-exact")
            .arg("3")
            .arg("--fix-rs-include-name");
    })?;
    assert_contentful(&tmp_dir, "gen0.cc");
    assert_exists(&tmp_dir, "gen1.cc");
    assert_exists(&tmp_dir, "gen2.cc");
    assert_contentful(&tmp_dir, "gen0.include.rs");
    assert_exists(&tmp_dir, "gen1.include.rs");
    assert_exists(&tmp_dir, "gen2.include.rs");
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
    assert!(p.metadata().unwrap().len() > super::BLANK.len().try_into().unwrap());
}

fn assert_exists(outdir: &TempDir, fname: &str) {
    let p = outdir.path().join(fname);
    if !p.exists() {
        panic!("File {} didn't exist", p.to_string_lossy());
    }
}

fn assert_contains(outdir: &TempDir, fname: &str, pattern: &str) {
    let p = outdir.path().join(fname);
    let content = std::fs::read_to_string(&p).unwrap();
    eprintln!("content = {}", content);
    assert!(content.contains(pattern));
}
