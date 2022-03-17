// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use proc_macro2::TokenStream;
use quote::quote;
use std::path::PathBuf;

/// The strategy used to generate, and to find generated, files.
/// As standard, these are based off the OUT_DIR set by Cargo,
/// but our code can't assume it can read OUT_DIR as it may
/// be running in the rust-analyzer proc macro server where it's
/// not available. We need to give provision for custom locations
/// and we need our code generator build script to be able to pass
/// locations through to the proc macro by env vars.
///
/// On the whole this class concerns itself with directory names
/// and allows the actual file name to be determined elsewhere
/// (based on a hash of the contents of `include_cpp!`.) But
/// some types of build system need to know the precise file _name_
/// produced by the codegen phase and passed into the macro phase,
/// so we have some options for that. See `gen --help` for details.
pub enum FileLocationStrategy {
    Custom(PathBuf),
    FromAutocxxRsFile(PathBuf),
    FromAutocxxRs(PathBuf),
    FromOutDir(PathBuf),
    UnknownMaybeFromOutdir,
}

static BUILD_DIR_NAME: &str = "autocxx-build-dir";
static RS_DIR_NAME: &str = "rs";
static AUTOCXX_RS: &str = "AUTOCXX_RS";
static AUTOCXX_RS_FILE: &str = "AUTOCXX_RS_FILE";

impl FileLocationStrategy {
    pub fn new() -> Self {
        match std::env::var_os(AUTOCXX_RS_FILE) {
            Some(of) => FileLocationStrategy::FromAutocxxRsFile(PathBuf::from(of)),
            None => match std::env::var_os(AUTOCXX_RS) {
                None => match std::env::var_os("OUT_DIR") {
                    None => FileLocationStrategy::UnknownMaybeFromOutdir,
                    Some(od) => FileLocationStrategy::FromOutDir(PathBuf::from(od)),
                },
                Some(acrs) => FileLocationStrategy::FromAutocxxRs(PathBuf::from(acrs)),
            },
        }
    }

    pub fn new_custom(gen_dir: PathBuf) -> Self {
        FileLocationStrategy::Custom(gen_dir)
    }

    /// Make a macro to include a given generated Rust file name.
    /// This can't simply be calculated from `get_rs_dir` because
    /// of limitations in rust-analyzer.
    pub fn make_include(&self, fname: &str) -> TokenStream {
        match self {
            FileLocationStrategy::FromAutocxxRs(custom_dir) => {
                let fname = custom_dir.join(fname).to_str().unwrap().to_string();
                quote! {
                    include!( #fname );
                }
            }
            FileLocationStrategy::Custom(_) => panic!("Should never happen in the macro"),
            FileLocationStrategy::UnknownMaybeFromOutdir | FileLocationStrategy::FromOutDir(_) => {
                let fname = format!("/{}/{}/{}", BUILD_DIR_NAME, RS_DIR_NAME, fname);
                // rust-analyzer works better if we ask Rust to do the path
                // concatenation rather than doing it in proc-macro code.
                // proc-macro code does not itself have access to the value of
                // OUT_DIR, but if we put it into a macro like the below,
                // rust-analyzer can cope.
                quote! {
                    include!(concat!(env!("OUT_DIR"), #fname));
                }
            }
            FileLocationStrategy::FromAutocxxRsFile(fname) => {
                let fname = fname
                    .to_str()
                    .expect("AUTOCXX_RS_FILE environment variable contained non-UTF8 characters");
                quote! {
                    include!( #fname );
                }
            }
        }
    }

    fn get_gen_dir(&self, suffix: &str) -> PathBuf {
        let root = match self {
            FileLocationStrategy::Custom(gen_dir)
            | FileLocationStrategy::FromAutocxxRs(gen_dir) => gen_dir.clone(),
            FileLocationStrategy::FromOutDir(out_dir) => out_dir.join(BUILD_DIR_NAME),
            FileLocationStrategy::UnknownMaybeFromOutdir => {
                panic!("Could not determine OUT_DIR or AUTOCXX_RS dir")
            }
            FileLocationStrategy::FromAutocxxRsFile(_) => {
                panic!("It's invalid to set AUTOCXX_RS_FILE during the codegen phase.")
            }
        };
        root.join(suffix)
    }

    /// Location to generate Rust files.
    pub fn get_rs_dir(&self) -> PathBuf {
        self.get_gen_dir(RS_DIR_NAME)
    }

    /// Location to generate C++ header files.
    pub fn get_include_dir(&self) -> PathBuf {
        self.get_gen_dir("include")
    }

    /// Location to generate C++ code.
    pub fn get_cxx_dir(&self) -> PathBuf {
        self.get_gen_dir("cxx")
    }

    /// From a build script, inform cargo how to set environment variables
    /// to make them available to the procedural macro.
    pub fn set_cargo_env_vars_for_build(&self) {
        if let FileLocationStrategy::Custom(_) = self {
            println!(
                "cargo:rustc-env={}={}",
                AUTOCXX_RS,
                self.get_rs_dir().to_str().unwrap()
            );
        }
    }
}

impl Default for FileLocationStrategy {
    fn default() -> Self {
        Self::new()
    }
}
