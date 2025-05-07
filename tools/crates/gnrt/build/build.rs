// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

//! This `build.rs` file invokes `rustc --print=cfg <target triple>` to
//! discover how each of Chromium-supported target triples maps to
//! how `rustc` sets `#[cfg(target_os = "...")]`, etc.
//!
//! This is written as `build.rs` and not as a proc macro, because only
//! the former gets `RUSTC` environment variable set by `cargo`:
//! https://doc.rust-lang.org/cargo/reference/environment-variables.html#environment-variables-cargo-sets-for-build-scripts

use anyhow::{ensure, Result};
use heck::ToPascalCase;
use itertools::Itertools;
use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use std::{env, fs::File, io::Write, path::Path, process::Command};

fn write_pretty_file_fragment(mut w: impl Write, contents: TokenStream) -> Result<()> {
    let syn_file = syn::parse2(contents)?;
    write!(w, "{}", prettyplease::unparse(&syn_file))?;
    Ok(())
}

fn format_ident(s: &str) -> syn::Ident {
    format_ident!("{}", s.to_pascal_case())
}

/// Represents one line of output from `rustc --print=cfg <triple_name>`.
struct CfgLine {
    triple_name: &'static str,
    property_name: String,
    property_value: String,
}

fn get_cfg_lines(triple_name: &'static str) -> Result<Vec<CfgLine>> {
    let rustc = env::var_os("RUSTC").unwrap();
    let output =
        Command::new(&rustc).arg("--print=cfg").arg(format!("--target={triple_name}")).output()?;
    ensure!(
        output.status.success(),
        "`rustc` returned a non-zero exit status: {}",
        std::str::from_utf8(&output.stderr).unwrap_or("<non utf-8 stderr>"),
    );
    Ok(std::str::from_utf8(&output.stdout)?
        .lines()
        .filter_map(|line| {
            line.split_once("=")
                .filter(|(property_name, _)| property_name.starts_with("target_"))
                .map(|(property_name, property_value)| {
                    let property_name = property_name.to_string();
                    let property_value = property_value.trim_matches('"').to_string();
                    CfgLine { triple_name, property_name, property_value }
                })
        })
        .collect_vec())
}

fn generate_enum_for_named_property(
    cfg_lines: &[CfgLine],
    doc_string: TokenStream,
    enum_ident: TokenStream,
    property_name: &str,
) -> TokenStream {
    let triple_and_property_value_idents = cfg_lines
        .iter()
        .filter_map(|cfg| {
            if cfg.property_name == property_name {
                Some((format_ident(cfg.triple_name), format_ident(&cfg.property_value)))
            } else {
                None
            }
        })
        .collect_vec();

    let unique_property_value_idents = triple_and_property_value_idents
        .clone()
        .into_iter()
        .map(|(_, property_value)| property_value)
        .sorted()
        .unique()
        .collect_vec();
    let (triple_idents, property_value_idents) =
        triple_and_property_value_idents.into_iter().unzip::<_, _, Vec<_>, Vec<_>>();
    quote! {
        #doc_string
        #[derive(Clone, Copy, Debug, Eq, Hash, PartialEq)]
        pub enum #enum_ident {
            #( #unique_property_value_idents, )*
        }

        impl From<RustTargetTriple> for #enum_ident {
            fn from(value: RustTargetTriple) -> Self {
                match value {
                    #( RustTargetTriple :: #triple_idents
                        => #enum_ident :: #property_value_idents, )*
                }
            }
        }
    }
}

fn write_target_triples_rs(path: &Path) -> Result<()> {
    let mut output_file = File::create(path)?;

    println!("cargo::rerun-if-changed=../../../build/rust/known-target-triples.txt");
    let triple_names = include_str!("../../../../build/rust/known-target-triples.txt")
        .lines()
        .filter(|line| !line.starts_with('#'))
        .filter(|line| !line.trim().is_empty())
        .sorted()
        .collect_vec();
    let triple_idents = triple_names.iter().map(|triple| format_ident(triple)).collect_vec();
    let cfg_lines = triple_names
        .iter()
        .map(|&triple| get_cfg_lines(triple))
        .collect::<Result<Vec<_>>>()?
        .into_iter()
        .flatten()
        .collect_vec();

    let target_properties = {
        let tuple_elem0s = cfg_lines.iter().map(|cfg| &cfg.triple_name);
        let tuple_elem1s = cfg_lines.iter().map(|cfg| &cfg.property_name);
        let tuple_elem2s = cfg_lines.iter().map(|cfg| &cfg.property_value);
        let len = cfg_lines.len();
        quote! {
            /// Each tuple below represents one line of output from
            /// `rustc --print=cfg <triple_name>`, where:
            ///
            /// * The 0th tuple element is the name of the target triple
            ///   (e.g. `x86_64-pc-windows-msvc`)
            /// * The 1st tuple element is the name of a property (e.g. `target_env`)
            /// * The 2nd tuple element is the value of the property (e.g. `msvc`)
            pub static RUST_TRIPLE_PROPERTIES: [(&str, &str, &str); #len] = [
                #( (#tuple_elem0s, #tuple_elem1s, #tuple_elem2s), )*
            ];
        }
    };
    let target_arch = generate_enum_for_named_property(
        &cfg_lines,
        quote! {
            /// Target architecture as seen/reported by `rustc`.
        },
        quote! { RustTargetArch },
        "target_arch",
    );
    let target_os = generate_enum_for_named_property(
        &cfg_lines,
        quote! {
            /// Target OS as seen/reported by `rustc`.
        },
        quote! { RustTargetOs },
        "target_os",
    );

    write_pretty_file_fragment(
        &mut output_file,
        quote! {
            use anyhow::{anyhow, Result};
            use std::convert::From;
            use std::collections::HashSet;
            use std::str::FromStr;
            use std::sync::LazyLock;

            /// `RustTargetTriple` enum exhaustively names all target triples that are
            /// supported by Chromium when compiling Rust libraries.
            #[derive(Clone, Copy, Debug, Eq, Hash, Ord, PartialEq, PartialOrd)]
            pub enum RustTargetTriple {
                #( #triple_idents, )*
            }

            impl RustTargetTriple {
                pub fn as_triple_name(&self) -> &'static str {
                    match *self {
                        #( RustTargetTriple :: #triple_idents => #triple_names, )*
                    }
                }

                /// A set of all possible/known triples.
                pub fn all() -> &'static HashSet<RustTargetTriple> {
                    static ALL_TRIPLES: LazyLock<HashSet<RustTargetTriple>> =
                        LazyLock::new(|| {
                            [
                                #( RustTargetTriple :: #triple_idents, )*
                            ].into_iter().collect()
                        });
                    &ALL_TRIPLES
                }
            }

            impl FromStr for RustTargetTriple {
                type Err = anyhow::Error;
                fn from_str(input: &str) -> Result<RustTargetTriple> {
                    for (s, t) in &[
                        #( (#triple_names, RustTargetTriple::#triple_idents), )*
                    ] {
                        if input == *s {
                            return Ok(*t);
                        }
                    }
                    Err(anyhow!("Unrecognized target triple: `{input}`"))
                }
            }

            #target_arch
            #target_os

            #target_properties
        },
    )?;
    Ok(())
}

fn main() -> Result<()> {
    let out_dir = env::var_os("OUT_DIR").unwrap();
    let out_dir = Path::new(&out_dir);
    write_target_triples_rs(&out_dir.join("target_triple.rs"))?;
    Ok(())
}
