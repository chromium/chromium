// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// Removed for Chromium: autocxx-integration-tests transitively depends on cc
// which is not allowed. See https://crbug.com/1336017. Comment out this entire
// file because it uses autocxx_integration_tests.
//
// TODO(https://crbug.com/1336017): remove the dependency on cc and restore
// these tests.
//
// use std::{convert::TryInto, fs::File, io::Write, path::Path};
// 
// use indexmap::map::IndexMap as HashMap;
// 
// use assert_cmd::Command;
// use autocxx_integration_tests::{build_from_folder, RsFindMode};
// use itertools::Itertools;
// use tempfile::{tempdir, TempDir};
// 
// static MAIN_RS: &str = concat!(
//     include_str!("../../../demo/src/main.rs"),
//     "#[link(name = \"autocxx-demo\")]\nextern \"C\" {}"
// );
// static INPUT_H: &str = include_str!("../../../demo/src/input.h");
// static BLANK: &str = "// Blank autocxx placeholder";
// 
// static MAIN2_RS: &str = concat!(
//     include_str!("data/main2.rs"),
//     "#[link(name = \"autocxx-demo\")]\nextern \"C\" {}"
// );
// static DIRECTIVE1_RS: &str = include_str!("data/directive1.rs");
// static DIRECTIVE2_RS: &str = include_str!("data/directive2.rs");
// static INPUT2_H: &str = include_str!("data/input2.h");
// static INPUT3_H: &str = include_str!("data/input3.h");
// 
// const KEEP_TEMPDIRS: bool = true;
// 
// #[test]
// fn test_help() -> Result<(), Box<dyn std::error::Error>> {
//     let mut cmd = Command::cargo_bin("autocxx-gen")?;
//     cmd.arg("-h").assert().success();
//     Ok(())
// }
// 
// enum RsGenMode {
//     Single,
//     Archive,
// }
// 
// fn base_test<F>(
//     tmp_dir: &TempDir,
//     rs_gen_mode: RsGenMode,
//     arg_modifier: F,
// ) -> Result<(), Box<dyn std::error::Error>>
// where
//     F: FnOnce(&mut Command),
// {
//     let mut standard_files = HashMap::new();
//     standard_files.insert("input.h", INPUT_H.as_bytes());
//     standard_files.insert("main.rs", MAIN_RS.as_bytes());
//     let result = base_test_ex(
//         tmp_dir,
//         rs_gen_mode,
//         arg_modifier,
//         standard_files,
//         vec!["main.rs"],
//     );
//     assert_contentful(tmp_dir, "gen0.cc");
//     result
// }
// 
// fn base_test_ex<F>(
//     tmp_dir: &TempDir,
//     rs_gen_mode: RsGenMode,
//     arg_modifier: F,
//     files_to_write: HashMap<&str, &[u8]>,
//     files_to_process: Vec<&str>,
// ) -> Result<(), Box<dyn std::error::Error>>
// where
//     F: FnOnce(&mut Command),
// {
//     let demo_code_dir = tmp_dir.path().join("demo");
//     std::fs::create_dir(&demo_code_dir).unwrap();
//     for (filename, content) in files_to_write {
//         write_to_file(&demo_code_dir, filename, content);
//     }
//     let mut cmd = Command::cargo_bin("autocxx-gen")?;
//     arg_modifier(&mut cmd);
//     cmd.arg("--inc")
//         .arg(demo_code_dir.to_str().unwrap())
//         .arg("--outdir")
//         .arg(tmp_dir.path().to_str().unwrap())
//         .arg("--gen-cpp");
//     cmd.arg(match rs_gen_mode {
//         RsGenMode::Single => "--gen-rs-include",
//         RsGenMode::Archive => "--gen-rs-archive",
//     });
//     for file in files_to_process {
//         cmd.arg(demo_code_dir.join(file));
//     }
//     let output = cmd.output();
//     if let Ok(output) = output {
//         eprintln!("Cmd stdout: {:?}", std::str::from_utf8(&output.stdout));
//         eprintln!("Cmd stderr: {:?}", std::str::from_utf8(&output.stderr));
//     }
//     cmd.assert().success();
//     Ok(())
// }
// 
// #[test]
// fn test_gen() -> Result<(), Box<dyn std::error::Error>> {
//     let tmp_dir = tempdir()?;
//     base_test(&tmp_dir, RsGenMode::Single, |_| {})?;
//     File::create(tmp_dir.path().join("cxx.h"))
//         .and_then(|mut cxx_h| cxx_h.write_all(autocxx_engine::HEADER.as_bytes()))?;
//     std::env::set_var("OUT_DIR", tmp_dir.path().to_str().unwrap());
//     let r = build_from_folder(
//         tmp_dir.path(),
//         &tmp_dir.path().join("demo/main.rs"),
//         vec![tmp_dir.path().join("autocxx-ffi-default-gen.rs")],
//         &["gen0.cc"],
//         RsFindMode::AutocxxRs,
//     );
//     if KEEP_TEMPDIRS {
//         println!("Tempdir: {:?}", tmp_dir.into_path().to_str());
//     }
//     r.unwrap();
//     Ok(())
// }
// 
// #[test]
// fn test_gen_archive() -> Result<(), Box<dyn std::error::Error>> {
//     let tmp_dir = tempdir()?;
//     base_test(&tmp_dir, RsGenMode::Archive, |_| {})?;
//     File::create(tmp_dir.path().join("cxx.h"))
//         .and_then(|mut cxx_h| cxx_h.write_all(autocxx_engine::HEADER.as_bytes()))?;
//     let r = build_from_folder(
//         tmp_dir.path(),
//         &tmp_dir.path().join("demo/main.rs"),
//         vec![tmp_dir.path().join("gen.rs.json")],
//         &["gen0.cc"],
//         RsFindMode::AutocxxRsArchive,
//     );
//     if KEEP_TEMPDIRS {
//         println!("Tempdir: {:?}", tmp_dir.into_path().to_str());
//     }
//     r.unwrap();
//     Ok(())
// }
// 
// #[test]
// fn test_gen_multiple_in_archive() -> Result<(), Box<dyn std::error::Error>> {
//     let tmp_dir = tempdir()?;
// 
//     let mut files = HashMap::new();
//     files.insert("input2.h", INPUT2_H.as_bytes());
//     files.insert("input3.h", INPUT3_H.as_bytes());
//     files.insert("main.rs", MAIN2_RS.as_bytes());
//     files.insert("directive1.rs", DIRECTIVE1_RS.as_bytes());
//     files.insert("directive2.rs", DIRECTIVE2_RS.as_bytes());
//     base_test_ex(
//         &tmp_dir,
//         RsGenMode::Archive,
//         |cmd| {
//             cmd.arg("--generate-exact").arg("8");
//         },
//         files,
//         vec!["directive1.rs", "directive2.rs"],
//     )?;
//     File::create(tmp_dir.path().join("cxx.h"))
//         .and_then(|mut cxx_h| cxx_h.write_all(autocxx_engine::HEADER.as_bytes()))?;
//     // We've asked to create 8 C++ files, mostly blank. Build 'em all.
//     let cpp_files = (0..7).map(|id| format!("gen{}.cc", id)).collect_vec();
//     let cpp_files = cpp_files.iter().map(|s| s.as_str()).collect_vec();
//     let r = build_from_folder(
//         tmp_dir.path(),
//         &tmp_dir.path().join("demo/main.rs"),
//         vec![tmp_dir.path().join("gen.rs.json")],
//         &cpp_files,
//         RsFindMode::AutocxxRsArchive,
//     );
//     if KEEP_TEMPDIRS {
//         println!("Tempdir: {:?}", tmp_dir.into_path().to_str());
//     }
//     r.unwrap();
//     Ok(())
// }
// 
// #[test]
// fn test_include_prefixes() -> Result<(), Box<dyn std::error::Error>> {
//     let tmp_dir = tempdir()?;
//     base_test(&tmp_dir, RsGenMode::Single, |cmd| {
//         cmd.arg("--cxx-h-path")
//             .arg("foo/")
//             .arg("--cxxgen-h-path")
//             .arg("bar/")
//             .arg("--generate-exact")
//             .arg("3")
//             .arg("--fix-rs-include-name");
//     })?;
//     assert_contains(&tmp_dir, "autocxxgen0.h", "foo/cxx.h");
//     // Currently we don't test cxxgen-h-path because we build the demo code
//     // which doesn't refer to generated cxx header code.
//     Ok(())
// }
// 
// #[test]
// fn test_gen_fixed_num() -> Result<(), Box<dyn std::error::Error>> {
//     let tmp_dir = tempdir()?;
//     let depfile = tmp_dir.path().join("test.d");
//     base_test(&tmp_dir, RsGenMode::Single, |cmd| {
//         cmd.arg("--generate-exact")
//             .arg("2")
//             .arg("--fix-rs-include-name")
//             .arg("--depfile")
//             .arg(depfile);
//     })?;
//     assert_contentful(&tmp_dir, "gen0.cc");
//     assert_contentful(&tmp_dir, "gen0.h");
//     assert_not_contentful(&tmp_dir, "gen1.cc");
//     assert_contentful(&tmp_dir, "autocxxgen0.h");
//     assert_not_contentful(&tmp_dir, "gen1.h");
//     assert_not_contentful(&tmp_dir, "autocxxgen1.h");
//     assert_contentful(&tmp_dir, "gen0.include.rs");
//     assert_contentful(&tmp_dir, "test.d");
//     File::create(tmp_dir.path().join("cxx.h"))
//         .and_then(|mut cxx_h| cxx_h.write_all(autocxx_engine::HEADER.as_bytes()))?;
//     let r = build_from_folder(
//         tmp_dir.path(),
//         &tmp_dir.path().join("demo/main.rs"),
//         vec![tmp_dir.path().join("gen0.include.rs")],
//         &["gen0.cc"],
//         RsFindMode::AutocxxRsFile,
//     );
//     if KEEP_TEMPDIRS {
//         println!("Tempdir: {:?}", tmp_dir.into_path().to_str());
//     }
//     r.unwrap();
//     Ok(())
// }
// 
// #[test]
// fn test_gen_preprocess() -> Result<(), Box<dyn std::error::Error>> {
//     let tmp_dir = tempdir()?;
//     let prepro_path = tmp_dir.path().join("preprocessed.h");
//     base_test(&tmp_dir, RsGenMode::Single, |cmd| {
//         cmd.env("AUTOCXX_PREPROCESS", prepro_path.to_str().unwrap());
//     })?;
//     assert_contentful(&tmp_dir, "preprocessed.h");
//     // Check that a random thing from one of the headers in
//     // `ALL_KNOWN_SYSTEM_HEADERS` is included.
//     assert!(std::fs::read_to_string(prepro_path)?.contains("integer_sequence"));
//     Ok(())
// }
// 
// #[test]
// fn test_gen_repro() -> Result<(), Box<dyn std::error::Error>> {
//     let tmp_dir = tempdir()?;
//     let repro_path = tmp_dir.path().join("repro.json");
//     base_test(&tmp_dir, RsGenMode::Single, |cmd| {
//         cmd.env("AUTOCXX_REPRO_CASE", repro_path.to_str().unwrap());
//     })?;
//     assert_contentful(&tmp_dir, "repro.json");
//     // Check that a random thing from one of the headers in
//     // `ALL_KNOWN_SYSTEM_HEADERS` is included.
//     assert!(std::fs::read_to_string(repro_path)?.contains("integer_sequence"));
//     Ok(())
// }
// 
// fn write_to_file(dir: &Path, filename: &str, content: &[u8]) {
//     let path = dir.join(filename);
//     let mut f = File::create(&path).expect("Unable to create file");
//     f.write_all(content).expect("Unable to write file");
// }
// 
// fn assert_contentful(outdir: &TempDir, fname: &str) {
//     let p = outdir.path().join(fname);
//     if !p.exists() {
//         panic!("File {} didn't exist", p.to_string_lossy());
//     }
//     assert!(
//         p.metadata().unwrap().len() > BLANK.len().try_into().unwrap(),
//         "File {} is empty",
//         fname
//     );
// }
// 
// fn assert_not_contentful(outdir: &TempDir, fname: &str) {
//     let p = outdir.path().join(fname);
//     if !p.exists() {
//         panic!("File {} didn't exist", p.to_string_lossy());
//     }
//     assert!(
//         p.metadata().unwrap().len() <= BLANK.len().try_into().unwrap(),
//         "File {} is not empty; it contains {}",
//         fname,
//         std::fs::read_to_string(&p).unwrap_or_default()
//     );
// }
// 
// fn assert_contains(outdir: &TempDir, fname: &str, pattern: &str) {
//     let p = outdir.path().join(fname);
//     let content = std::fs::read_to_string(&p).expect(fname);
//     eprintln!("content = {}", content);
//     assert!(content.contains(pattern));
// }
// 