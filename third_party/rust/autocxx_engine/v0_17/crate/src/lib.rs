//! The core of the `autocxx` engine, used by both the
//! `autocxx_macro` and also code generators (e.g. `autocxx_build`).
//! See [IncludeCppEngine] for general description of how this engine works.

// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

// This feature=nightly could be set by build.rs, but since we only care
// about it for docs, we ask docs.rs to set it in the Cargo.toml.
#![cfg_attr(feature = "nightly", feature(doc_cfg))]
#![forbid(unsafe_code)]

mod ast_discoverer;
mod conversion;
mod cxxbridge;
mod known_types;
mod parse_callbacks;
mod parse_file;
mod rust_pretty_printer;
mod types;

#[cfg(any(test, feature = "build"))]
mod builder;

use autocxx_parser::{IncludeCppConfig, UnsafePolicy};
use conversion::BridgeConverter;
use parse_callbacks::AutocxxParseCallbacks;
use parse_file::CppBuildable;
use proc_macro2::TokenStream as TokenStream2;
use std::{fmt::Display, path::PathBuf};
use std::{
    fs::File,
    io::prelude::*,
    path::Path,
    process::{Command, Stdio},
};
use tempfile::NamedTempFile;

use quote::ToTokens;
use syn::Result as ParseResult;
use syn::{
    parse::{Parse, ParseStream},
    parse_quote, ItemMod, Macro,
};

use itertools::{join, Itertools};
use known_types::known_types;
use log::info;

/// We use a forked version of bindgen - for now.
/// We hope to unfork.
use autocxx_bindgen as bindgen;

#[cfg(any(test, feature = "build"))]
pub use builder::{
    Builder, BuilderBuild, BuilderContext, BuilderError, BuilderResult, BuilderSuccess,
};
pub use parse_file::{parse_file, ParseError, ParsedFile};

pub use cxx_gen::HEADER;

#[derive(Clone)]
/// Some C++ content which should be written to disk and built.
pub struct CppFilePair {
    /// Declarations to go into a header file.
    pub header: Vec<u8>,
    /// Implementations to go into a .cpp file.
    pub implementation: Option<Vec<u8>>,
    /// The name which should be used for the header file
    /// (important as it may be `#include`d elsewhere)
    pub header_name: String,
}

/// All generated C++ content which should be written to disk.
pub struct GeneratedCpp(pub Vec<CppFilePair>);

/// Errors which may occur in generating bindings for these C++
/// functions.
#[derive(Debug)]
pub enum Error {
    /// Any error reported by bindgen, generating the C++ bindings.
    /// Any C++ parsing errors, etc. would be reported this way.
    Bindgen(()),
    /// Any problem parsing the Rust file.
    Parsing(syn::Error),
    /// No `include_cpp!` macro could be found.
    NoAutoCxxInc,
    /// Some error occcurred in converting the bindgen-style
    /// bindings to safe cxx bindings.
    Conversion(conversion::ConvertError),
}

impl Display for Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Error::Bindgen(_) => write!(f, "Bindgen was unable to generate the initial .rs bindings for this file. This may indicate a parsing problem with the C++ headers.")?,
            Error::Parsing(err) => write!(f, "The Rust file could not be parsed: {}", err)?,
            Error::NoAutoCxxInc => write!(f, "No C++ include directory was provided.")?,
            Error::Conversion(err) => write!(f, "autocxx could not generate the requested bindings. {}", err)?,
        }
        Ok(())
    }
}

/// Result type.
pub type Result<T, E = Error> = std::result::Result<T, E>;

struct GenerationResults {
    item_mod: ItemMod,
    cpp: Option<CppFilePair>,
    inc_dirs: Vec<PathBuf>,
}
enum State {
    NotGenerated,
    ParseOnly,
    Generated(Box<GenerationResults>),
}

const AUTOCXX_CLANG_ARGS: &[&str; 4] = &["-x", "c++", "-std=c++14", "-DBINDGEN"];

/// Implement to learn of header files which get included
/// by this build process, such that your build system can choose
/// to rerun the build process if any such file changes in future.
pub trait RebuildDependencyRecorder: std::fmt::Debug {
    /// Records that this autocxx build depends on the given
    /// header file. Full paths will be provided.
    fn record_header_file_dependency(&self, filename: &str);
}

#[cfg_attr(doc, aquamarine::aquamarine)]
/// Core of the autocxx engine.
///
/// The basic idea is this. We will run `bindgen` which will spit
/// out a ton of Rust code corresponding to all the types and functions
/// defined in C++. We'll then post-process that bindgen output
/// into a form suitable for ingestion by `cxx`.
/// (It's the `BridgeConverter` mod which does that.)
/// Along the way, the `bridge_converter` might tell us of additional
/// C++ code which we should generate, e.g. wrappers to move things
/// into and out of `UniquePtr`s.
///
/// ```mermaid
/// flowchart TB
///     s[(C++ headers)]
///     s --> lc
///     rss[(.rs input)]
///     rss --> parser
///     parser --> include_cpp_conf
///     cpp_output[(C++ output)]
///     rs_output[(.rs output)]
///     subgraph autocxx[autocxx_engine]
///     parser[File parser]
///     subgraph bindgen[autocxx_bindgen]
///     lc[libclang parse]
///     bir(bindgen IR)
///     lc --> bir
///     end
///     bgo(bindgen generated bindings)
///     bir --> bgo
///     include_cpp_conf(Config from include_cpp)
///     syn[Parse with syn]
///     bgo --> syn
///     conv[['conversion' mod: see below]]
///     syn --> conv
///     rsgen(Generated .rs TokenStream)
///     conv --> rsgen
///     subgraph cxx_gen
///     cxx_codegen[cxx_gen C++ codegen]
///     end
///     rsgen --> cxx_codegen
///     end
///     conv -- autocxx C++ codegen --> cpp_output
///     rsgen -- autocxx .rs codegen --> rs_output
///     cxx_codegen -- cxx C++ codegen --> cpp_output
///     subgraph rustc [rustc build]
///     subgraph autocxx_macro
///     include_cpp[autocxx include_cpp macro]
///     end
///     subgraph cxx
///     cxxm[cxx procedural macro]
///     end
///     comprs(Fully expanded Rust code)
///     end
///     rs_output-. included .->include_cpp
///     include_cpp --> cxxm
///     cxxm --> comprs
///     rss --> rustc
///     include_cpp_conf -. used to configure .-> bindgen
///     include_cpp_conf --> conv
///     link[linker]
///     cpp_output --> link
///     comprs --> link
/// ```
///
/// Here's a zoomed-in view of the "conversion" part:
///
/// ```mermaid
/// flowchart TB
///     syn[(syn parse)]
///     apis(Unanalyzed APIs)
///     subgraph parse
///     syn ==> parse_bindgen
///     end
///     parse_bindgen ==> apis
///     subgraph analysis
///     typedef[typedef analysis]
///     pod[POD analysis]
///     apis ==> typedef
///     typedef ==> pod
///     podapis(APIs with POD analysis)
///     pod ==> podapis
///     fun[Function materialization analysis]
///     podapis ==> fun
///     funapis(APIs with function analysis)
///     fun ==> funapis
///     gc[Garbage collection]
///     funapis ==> gc
///     ctypes[C int analysis]
///     gc ==> ctypes
///     ctypes ==> finalapis
///     end
///     finalapis(Analyzed APIs)
///     codegenrs(.rs codegen)
///     codegencpp(.cpp codegen)
///     finalapis ==> codegenrs
///     finalapis ==> codegencpp
/// ```
pub struct IncludeCppEngine {
    config: IncludeCppConfig,
    state: State,
}

impl Parse for IncludeCppEngine {
    fn parse(input: ParseStream) -> ParseResult<Self> {
        let config = input.parse::<IncludeCppConfig>()?;
        let state = if config.parse_only {
            State::ParseOnly
        } else {
            State::NotGenerated
        };
        Ok(Self { config, state })
    }
}

impl IncludeCppEngine {
    pub fn new_from_syn(mac: Macro) -> Result<Self> {
        mac.parse_body::<IncludeCppEngine>().map_err(Error::Parsing)
    }

    pub fn config_mut(&mut self) -> &mut IncludeCppConfig {
        assert!(
            matches!(self.state, State::NotGenerated),
            "Can't alter config after generation commenced"
        );
        &mut self.config
    }

    fn build_header(&self) -> String {
        join(
            self.config
                .inclusions
                .iter()
                .map(|path| format!("#include \"{}\"\n", path)),
            "",
        )
    }

    fn make_bindgen_builder(
        &self,
        inc_dirs: &[PathBuf],
        extra_clang_args: &[&str],
    ) -> bindgen::Builder {
        let mut builder = bindgen::builder()
            .clang_args(make_clang_args(inc_dirs, extra_clang_args))
            .derive_copy(false)
            .derive_debug(false)
            .default_enum_style(bindgen::EnumVariation::Rust {
                non_exhaustive: false,
            })
            .enable_cxx_namespaces()
            .generate_inline_functions(true)
            .respect_cxx_access_specs(true)
            .use_specific_virtual_function_receiver(true)
            .cpp_semantic_attributes(true)
            .represent_cxx_operators(true)
            .layout_tests(false); // TODO revisit later
        for item in known_types().get_initial_blocklist() {
            builder = builder.blocklist_item(item);
        }

        // 3. Passes allowlist and other options to the bindgen::Builder equivalent
        //    to --output-style=cxx --allowlist=<as passed in>
        if let Some(allowlist) = self.config.bindgen_allowlist() {
            for a in allowlist {
                // TODO - allowlist type/functions/separately
                builder = builder
                    .allowlist_type(&a)
                    .allowlist_function(&a)
                    .allowlist_var(&a);
            }
        }

        log::info!(
            "Bindgen flags would be: {}",
            builder
                .command_line_flags()
                .into_iter()
                .map(|f| format!("\"{}\"", f))
                .join(" ")
        );
        builder
    }

    pub fn get_rs_filename(&self) -> String {
        self.config.get_rs_filename()
    }

    /// Generate the Rust bindings. Call `generate` first.
    pub fn generate_rs(&self) -> TokenStream2 {
        match &self.state {
            State::NotGenerated => panic!("Generate first"),
            State::Generated(gen_results) => gen_results.item_mod.to_token_stream(),
            State::ParseOnly => TokenStream2::new(),
        }
    }

    /// Returns the name of the mod which this `include_cpp!` will generate.
    /// Can and should be used to ensure multiple mods in a file don't conflict.
    pub fn get_mod_name(&self) -> String {
        self.config.get_mod_name().to_string()
    }

    fn parse_bindings(&self, bindings: bindgen::Bindings) -> Result<ItemMod> {
        // This bindings object is actually a TokenStream internally and we're wasting
        // effort converting to and from string. We could enhance the bindgen API
        // in future.
        let bindings = bindings.to_string();
        // Manually add the mod ffi {} so that we can ask syn to parse
        // into a single construct.
        let bindings = format!("mod bindgen {{ {} }}", bindings);
        info!("Bindings: {}", bindings);
        syn::parse_str::<ItemMod>(&bindings).map_err(Error::Parsing)
    }

    /// Actually examine the headers to find out what needs generating.
    /// Most errors occur at this stage as we fail to interpret the C++
    /// headers properly.
    ///
    /// See documentation for this type for flow diagrams and more details.
    pub fn generate(
        &mut self,
        inc_dirs: Vec<PathBuf>,
        extra_clang_args: &[&str],
        dep_recorder: Option<Box<dyn RebuildDependencyRecorder>>,
        cpp_codegen_options: &CppCodegenOptions,
    ) -> Result<()> {
        // If we are in parse only mode, do nothing. This is used for
        // doc tests to ensure the parsing is valid, but we can't expect
        // valid C++ header files or linkers to allow a complete build.
        match self.state {
            State::ParseOnly => return Ok(()),
            State::NotGenerated => {}
            State::Generated(_) => panic!("Only call generate once"),
        }

        let mod_name = self.config.get_mod_name();
        let mut builder = self.make_bindgen_builder(&inc_dirs, extra_clang_args);
        if let Some(dep_recorder) = dep_recorder {
            builder = builder.parse_callbacks(Box::new(AutocxxParseCallbacks(dep_recorder)));
        }
        let header_contents = self.build_header();
        self.dump_header_if_so_configured(&header_contents, &inc_dirs, extra_clang_args);
        let header_and_prelude = format!("{}\n\n{}", known_types().get_prelude(), header_contents);
        log::info!("Header and prelude for bindgen:\n{}", header_and_prelude);
        builder = builder.header_contents("example.hpp", &header_and_prelude);

        let bindings = builder.generate().map_err(Error::Bindgen)?;
        let bindings = self.parse_bindings(bindings)?;

        let converter = BridgeConverter::new(&self.config.inclusions, &self.config);

        let conversion = converter
            .convert(
                bindings,
                self.config.unsafe_policy.clone(),
                header_contents,
                cpp_codegen_options,
            )
            .map_err(Error::Conversion)?;
        let mut items = conversion.rs;
        let mut new_bindings: ItemMod = parse_quote! {
            #[allow(non_snake_case)]
            #[allow(dead_code)]
            #[allow(non_upper_case_globals)]
            #[allow(non_camel_case_types)]
            mod #mod_name {
            }
        };
        new_bindings.content.as_mut().unwrap().1.append(&mut items);
        info!(
            "New bindings:\n{}",
            rust_pretty_printer::pretty_print(&new_bindings.to_token_stream())
        );
        self.state = State::Generated(Box::new(GenerationResults {
            item_mod: new_bindings,
            cpp: conversion.cpp,
            inc_dirs,
        }));
        Ok(())
    }

    /// Return the include directories used for this include_cpp invocation.
    fn include_dirs(&self) -> impl Iterator<Item = &PathBuf> {
        match &self.state {
            State::Generated(gen_results) => gen_results.inc_dirs.iter(),
            _ => panic!("Must call generate() before include_dirs()"),
        }
    }

    fn dump_header_if_so_configured(
        &self,
        header: &str,
        inc_dirs: &[PathBuf],
        extra_clang_args: &[&str],
    ) {
        if let Ok(output_path) = std::env::var("AUTOCXX_PREPROCESS") {
            self.make_preprocessed_file(
                &PathBuf::from(output_path),
                header,
                inc_dirs,
                extra_clang_args,
            );
        }
        #[cfg(feature = "reproduction_case")]
        if let Ok(output_path) = std::env::var("AUTOCXX_REPRO_CASE") {
            let tf = NamedTempFile::new().unwrap();
            self.make_preprocessed_file(
                &PathBuf::from(tf.path()),
                header,
                inc_dirs,
                extra_clang_args,
            );
            let header = std::fs::read_to_string(tf.path()).unwrap();
            let output_path = PathBuf::from(output_path);
            let config = self.config.to_token_stream().to_string();
            let json = serde_json::json!({
                "header": header,
                "config": config
            });
            let f = File::create(&output_path).unwrap();
            serde_json::to_writer(f, &json).unwrap();
        }
    }

    fn make_preprocessed_file(
        &self,
        output_path: &Path,
        header: &str,
        inc_dirs: &[PathBuf],
        extra_clang_args: &[&str],
    ) {
        // Include a load of system headers at the end of the preprocessed output,
        // because we would like to be able to generate bindings from the
        // preprocessed header, and then build those bindings. The C++ parts
        // of those bindings might need things inside these various headers;
        // we make sure all these definitions and declarations are inside
        // this one header file so that the reduction process does not have
        // to refer to local headers on the reduction machine too.
        let suffix = ALL_KNOWN_SYSTEM_HEADERS
            .iter()
            .map(|hdr| format!("#include <{}>\n", hdr))
            .join("\n");
        let input = format!("/*\nautocxx config:\n\n{:?}\n\nend autocxx config.\nautocxx preprocessed input:\n*/\n\n{}\n\n/* autocxx: extra headers added below for completeness. */\n\n{}\n{}\n",
            self.config, header, suffix, cxx_gen::HEADER);
        let mut tf = NamedTempFile::new().unwrap();
        write!(tf, "{}", input).unwrap();
        let tp = tf.into_temp_path();
        preprocess(&tp, &PathBuf::from(output_path), inc_dirs, extra_clang_args).unwrap();
    }
}

/// This is a list of all the headers known to be included in generated
/// C++ by cxx. We only use this when `AUTOCXX_PERPROCESS` is set to true,
/// in an attempt to make the resulting preprocessed header more hermetic.
/// We clearly should _not_ use this in any other circumstance; obviously
/// we'd then want to add an API to cxx_gen such that we could retrieve
/// that information from source.
static ALL_KNOWN_SYSTEM_HEADERS: &[&str] = &[
    "memory",
    "string",
    "algorithm",
    "array",
    "cassert",
    "cstddef",
    "cstdint",
    "cstring",
    "exception",
    "functional",
    "initializer_list",
    "iterator",
    "memory",
    "new",
    "stdexcept",
    "type_traits",
    "utility",
    "vector",
    "sys/types.h",
];

pub fn do_cxx_cpp_generation(
    rs: TokenStream2,
    cpp_codegen_options: &CppCodegenOptions,
) -> Result<CppFilePair, cxx_gen::Error> {
    let mut opt = cxx_gen::Opt::default();
    opt.cxx_impl_annotations = cpp_codegen_options.cxx_impl_annotations.clone();
    let cxx_generated = cxx_gen::generate_header_and_cc(rs, &opt)?;
    Ok(CppFilePair {
        header: strip_system_headers(
            cxx_generated.header,
            cpp_codegen_options.suppress_system_headers,
        ),
        header_name: "cxxgen.h".into(),
        implementation: Some(strip_system_headers(
            cxx_generated.implementation,
            cpp_codegen_options.suppress_system_headers,
        )),
    })
}

pub(crate) fn strip_system_headers(input: Vec<u8>, suppress_system_headers: bool) -> Vec<u8> {
    if suppress_system_headers {
        std::str::from_utf8(&input)
            .unwrap()
            .lines()
            .filter(|l| !l.starts_with("#include <"))
            .join("\n")
            .as_bytes()
            .to_vec()
    } else {
        input
    }
}

impl CppBuildable for IncludeCppEngine {
    /// Generate C++-side bindings for these APIs. Call `generate` first.
    fn generate_h_and_cxx(
        &self,
        cpp_codegen_options: &CppCodegenOptions,
    ) -> Result<GeneratedCpp, cxx_gen::Error> {
        let mut files = Vec::new();
        match &self.state {
            State::ParseOnly => panic!("Cannot generate C++ in parse-only mode"),
            State::NotGenerated => panic!("Call generate() first"),
            State::Generated(gen_results) => {
                let rs = gen_results.item_mod.to_token_stream();
                if !cpp_codegen_options.skip_cxx_gen {
                    files.push(do_cxx_cpp_generation(rs, cpp_codegen_options)?);
                }
                if let Some(cpp_file_pair) = &gen_results.cpp {
                    files.push(cpp_file_pair.clone());
                }
            }
        };
        Ok(GeneratedCpp(files))
    }
}

/// Get clang args as if we were operating clang the same way as we operate
/// bindgen.
pub fn make_clang_args<'a>(
    incs: &'a [PathBuf],
    extra_args: &'a [&str],
) -> impl Iterator<Item = String> + 'a {
    // AUTOCXX_CLANG_ARGS come first so that any defaults defined there(e.g. for the `-std`
    // argument) can be overridden by extra_args.
    AUTOCXX_CLANG_ARGS
        .iter()
        .map(|s| s.to_string())
        .chain(incs.iter().map(|i| format!("-I{}", i.to_str().unwrap())))
        .chain(extra_args.iter().map(|s| s.to_string()))
}

/// Preprocess a file using the same options
/// as is used by autocxx. Input: listing_path, output: preprocess_path.
pub fn preprocess(
    listing_path: &Path,
    preprocess_path: &Path,
    incs: &[PathBuf],
    extra_clang_args: &[&str],
) -> Result<(), std::io::Error> {
    let mut cmd = Command::new(get_clang_path());
    cmd.arg("-E");
    cmd.arg("-C");
    cmd.args(make_clang_args(incs, extra_clang_args));
    cmd.arg(listing_path.to_str().unwrap());
    cmd.stderr(Stdio::inherit());
    let result = cmd.output().expect("failed to execute clang++");
    assert!(result.status.success(), "failed to preprocess");
    let mut file = File::create(preprocess_path)?;
    file.write_all(&result.stdout)?;
    Ok(())
}

/// Get the path to clang which is effective for any preprocessing
/// operations done by autocxx.
pub fn get_clang_path() -> String {
    // `CLANG_PATH` is the environment variable that clang-sys uses to specify
    // the path to Clang, so in most cases where someone is using a compiler
    // that's not on the path, things should just work. We also check `CXX`,
    // since some users may have set that.
    std::env::var("CLANG_PATH")
        .or_else(|_| std::env::var("CXX"))
        .unwrap_or_else(|_| "clang++".to_string())
}

/// Newtype wrapper so we can give it a [`Default`].
pub struct HeaderNamer<'a>(pub Box<dyn 'a + Fn(String) -> String>);

impl Default for HeaderNamer<'static> {
    fn default() -> Self {
        Self(Box::new(|mod_name| format!("autocxxgen_{}.h", mod_name)))
    }
}

impl HeaderNamer<'_> {
    fn name_header(&self, mod_name: String) -> String {
        self.0(mod_name)
    }
}

/// Options for C++ codegen
#[derive(Default)]
pub struct CppCodegenOptions<'a> {
    /// Whether to avoid generating `#include <some-system-header>`.
    /// You may wish to do this to make a hermetic test case with no
    /// external dependencies.
    pub suppress_system_headers: bool,
    /// Optionally, a prefix to go at `#include "<here>cxx.h". This is a header file from the `cxx`
    /// crate.
    pub path_to_cxx_h: Option<String>,
    /// Optionally, a prefix to go at `#include "<here>cxxgen.h". This is a header file which we
    /// generate.
    pub path_to_cxxgen_h: Option<String>,
    /// Optionally, a function called to generate each of the per-section header files. The default
    /// names are subject to change.
    /// The function is passed the name of the module generated by each `include_cpp`,
    /// configured via `name`. These will be unique.
    pub header_namer: HeaderNamer<'a>,
    /// An annotation optionally to include on each C++ function.
    /// For example to export the symbol from a library.
    pub cxx_impl_annotations: Option<String>,
    /// Whether to skip using [`cxx_gen`] to generate the C++ code,
    /// so that some other process can handle that.
    pub skip_cxx_gen: bool,
}
