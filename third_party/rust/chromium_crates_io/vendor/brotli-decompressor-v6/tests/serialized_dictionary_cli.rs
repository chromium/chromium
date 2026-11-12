#![cfg(not(feature = "seccomp"))]

use std::fs;
use std::path::{Path, PathBuf};
use std::process::Command;

fn fixture(name: &str) -> PathBuf {
  Path::new(env!("CARGO_MANIFEST_DIR")).join("testdata").join(name)
}

fn temp_path(suffix: &str) -> PathBuf {
  std::env::temp_dir().join(format!(
      "brotli-decompressor-cli-{}-{}", std::process::id(), suffix))
}

#[test]
fn serialized_dictionary_flag_decodes_reference_stream() {
  let output_path = temp_path("shared-output");
  let status = Command::new(env!("CARGO_BIN_EXE_brotli-decompressor"))
      .arg(format!("-serialized_dict={}",
                   fixture("shared_custom.dict").display()))
      .arg(fixture("shared_custom.compressed"))
      .arg(&output_path)
      .status()
      .expect("run brotli-decompressor");
  assert!(status.success());
  assert_eq!(fs::read(&output_path).unwrap(),
             fs::read(fixture("shared_content")).unwrap());
  fs::remove_file(output_path).unwrap();
}

#[test]
fn empty_serialized_dictionary_argument_is_not_silently_ignored() {
  let empty_path = temp_path("empty-dictionary");
  let output_path = temp_path("empty-output");
  fs::write(&empty_path, &[]).unwrap();
  let output = Command::new(env!("CARGO_BIN_EXE_brotli-decompressor"))
      .arg(format!("-serialized_dict={}", empty_path.display()))
      .arg(fixture("alice29.txt.compressed"))
      .arg(&output_path)
      .output()
      .expect("run brotli-decompressor");
  assert!(!output.status.success());
  fs::remove_file(empty_path).unwrap();
  if output_path.exists() {
    fs::remove_file(output_path).unwrap();
  }
}
