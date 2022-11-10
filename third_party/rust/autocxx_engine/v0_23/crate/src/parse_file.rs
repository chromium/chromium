// Copyright 2020 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use crate::ast_discoverer::{Discoveries, DiscoveryErr};
use crate::output_generators::RsOutput;
use crate::{
    cxxbridge::CxxBridge, Error as EngineError, GeneratedCpp, IncludeCppEngine,
    RebuildDependencyRecorder,
};
use crate::{CppCodegenOptions, LocatedSynError};
use autocxx_parser::directive_names::SUBCLASS;
use autocxx_parser::{AllowlistEntry, RustPath, Subclass, SubclassAttrs};
use indexmap::set::IndexSet as HashSet;
use miette::Diagnostic;
use quote::ToTokens;
use std::{io::Read, path::PathBuf};
use std::{panic::UnwindSafe, path::Path, rc::Rc};
use syn::{token::Brace, Item, ItemMod};
use thiserror::Error;

/// Errors which may occur when parsing a Rust source file to discover
/// and interpret include_cxx macros.
#[derive(Error, Diagnostic, Debug)]
pub enum ParseError {
    #[error("unable to open the source file: {0}")]
    FileOpen(std::io::Error),
    #[error("the .rs file couldn't be read: {0}")]
    FileRead(std::io::Error),
    #[error("syntax error interpreting Rust code: {0}")]
    #[diagnostic(transparent)]
    Syntax(LocatedSynError),
    #[error("generate!/generate_ns! was used at the same time as generate_all!")]
    ConflictingAllowlist,
    #[error("the subclass attribute couldn't be parsed: {0}")]
    #[diagnostic(transparent)]
    SubclassSyntax(LocatedSynError),
    /// The include CPP macro could not be expanded into
    /// Rust bindings to C++, because of some problem during the conversion
    /// process. This could be anything from a C++ parsing error to some
    /// C++ feature that autocxx can't yet handle and isn't able to skip
    /// over. It could also cover errors in your syntax of the `include_cpp`
    /// macro or the directives inside.
    #[error("the include_cpp! macro couldn't be expanded into Rust bindings to C++: {0}")]
    #[diagnostic(transparent)]
    AutocxxCodegenError(EngineError),
    /// There are two or more `include_cpp` macros with the same
    /// mod name.
    #[error("there are two or more include_cpp! mods with the same mod name")]
    ConflictingModNames,
    #[error("dynamic discovery was enabled but no mod was found")]
    ZeroModsForDynamicDiscovery,
    #[error("dynamic discovery was enabled but multiple mods were found")]
    MultipleModsForDynamicDiscovery,
    #[error("a problem occurred while discovering C++ APIs used within the Rust: {0}")]
    Discovery(DiscoveryErr),
}

/// Parse a Rust file, and spot any include_cpp macros within it.
pub fn parse_file<P1: AsRef<Path>>(
    rs_file: P1,
    auto_allowlist: bool,
) -> Result<ParsedFile, ParseError> {
    let mut source_code = String::new();
    let mut file = std::fs::File::open(rs_file).map_err(ParseError::FileOpen)?;
    file.read_to_string(&mut source_code)
        .map_err(ParseError::FileRead)?;
    proc_macro2::fallback::force();
    let source = syn::parse_file(&source_code)
        .map_err(|e| ParseError::Syntax(LocatedSynError::new(e, &source_code)))?;
    parse_file_contents(source, auto_allowlist, &source_code)
}

fn parse_file_contents(
    source: syn::File,
    auto_allowlist: bool,
    file_contents: &str,
) -> Result<ParsedFile, ParseError> {
    #[derive(Default)]
    struct State {
        auto_allowlist: bool,
        results: Vec<Segment>,
        extra_superclasses: Vec<Subclass>,
        discoveries: Discoveries,
    }
    let file_contents = Rc::new(file_contents.to_string());
    impl State {
        fn parse_item(
            &mut self,
            item: Item,
            mod_path: Option<RustPath>,
            file_contents: Rc<String>,
        ) -> Result<(), ParseError> {
            let result = match item {
                Item::Macro(mac)
                    if mac
                        .mac
                        .path
                        .segments
                        .last()
                        .map(|s| s.ident == "include_cpp")
                        .unwrap_or(false) =>
                {
                    Segment::Autocxx(
                        crate::IncludeCppEngine::new_from_syn(mac.mac, file_contents)
                            .map_err(ParseError::AutocxxCodegenError)?,
                    )
                }
                Item::Mod(itm)
                    if itm
                        .attrs
                        .iter()
                        .any(|attr| attr.path.to_token_stream().to_string() == "cxx :: bridge") =>
                {
                    Segment::Cxx(CxxBridge::from(itm))
                }
                Item::Mod(itm) => {
                    if let Some((brace, items)) = itm.content {
                        let mut mod_state = State {
                            auto_allowlist: self.auto_allowlist,
                            ..Default::default()
                        };
                        let mod_path = match &mod_path {
                            None => RustPath::new_from_ident(itm.ident.clone()),
                            Some(mod_path) => mod_path.append(itm.ident.clone()),
                        };
                        for item in items {
                            mod_state.parse_item(
                                item,
                                Some(mod_path.clone()),
                                file_contents.clone(),
                            )?
                        }
                        self.extra_superclasses.extend(mod_state.extra_superclasses);
                        self.discoveries.extend(mod_state.discoveries);
                        Segment::Mod(
                            mod_state.results,
                            (
                                brace,
                                ItemMod {
                                    content: None,
                                    ..itm
                                },
                            ),
                        )
                    } else {
                        Segment::Other(Item::Mod(itm))
                    }
                }
                Item::Struct(ref its) if self.auto_allowlist => {
                    let attrs = &its.attrs;
                    let is_superclass_attr = attrs.iter().find(|attr| {
                        attr.path
                            .segments
                            .last()
                            .map(|seg| seg.ident == "is_subclass" || seg.ident == SUBCLASS)
                            .unwrap_or(false)
                    });
                    if let Some(is_superclass_attr) = is_superclass_attr {
                        if !is_superclass_attr.tokens.is_empty() {
                            let subclass = its.ident.clone();
                            let args: SubclassAttrs =
                                is_superclass_attr.parse_args().map_err(|e| {
                                    ParseError::SubclassSyntax(LocatedSynError::new(
                                        e,
                                        &file_contents,
                                    ))
                                })?;
                            if let Some(superclass) = args.superclass {
                                self.extra_superclasses.push(Subclass {
                                    superclass,
                                    subclass,
                                })
                            }
                        }
                    }
                    self.discoveries
                        .search_item(&item, mod_path)
                        .map_err(ParseError::Discovery)?;
                    Segment::Other(item)
                }
                _ => {
                    self.discoveries
                        .search_item(&item, mod_path)
                        .map_err(ParseError::Discovery)?;
                    Segment::Other(item)
                }
            };
            self.results.push(result);
            Ok(())
        }
    }
    let mut state = State {
        auto_allowlist,
        ..Default::default()
    };
    for item in source.items {
        state.parse_item(item, None, file_contents.clone())?
    }
    let State {
        auto_allowlist,
        mut results,
        mut extra_superclasses,
        mut discoveries,
    } = state;

    let must_handle_discovered_things = discoveries.found_rust()
        || !extra_superclasses.is_empty()
        || (auto_allowlist && discoveries.found_allowlist());

    // We do not want to enter this 'if' block unless the above conditions are true,
    // since we may emit errors.
    if must_handle_discovered_things {
        let mut autocxx_seg_iterator = results.iter_mut().filter_map(|seg| match seg {
            Segment::Autocxx(engine) => Some(engine),
            _ => None,
        });
        let our_seg = autocxx_seg_iterator.next();
        match our_seg {
            None => return Err(ParseError::ZeroModsForDynamicDiscovery),
            Some(engine) => {
                engine
                    .config_mut()
                    .subclasses
                    .append(&mut extra_superclasses);
                if auto_allowlist {
                    for cpp in discoveries.cpp_list {
                        engine
                            .config_mut()
                            .allowlist
                            .push(AllowlistEntry::Item(cpp))
                            .map_err(|_| ParseError::ConflictingAllowlist)?;
                    }
                }
                engine
                    .config_mut()
                    .extern_rust_funs
                    .append(&mut discoveries.extern_rust_funs);
                engine
                    .config_mut()
                    .rust_types
                    .append(&mut discoveries.extern_rust_types);
            }
        }
        if autocxx_seg_iterator.next().is_some() {
            return Err(ParseError::MultipleModsForDynamicDiscovery);
        }
    }
    let autocxx_seg_iterator = results.iter_mut().filter_map(|seg| match seg {
        Segment::Autocxx(engine) => Some(engine),
        _ => None,
    });
    for seg in autocxx_seg_iterator {
        seg.config.confirm_complete();
    }
    Ok(ParsedFile(results))
}

/// A Rust file parsed by autocxx. May contain zero or more autocxx 'engines',
/// i.e. the `IncludeCpp` class, corresponding to zero or more include_cpp
/// macros within this file. Also contains `syn::Item` structures for all
/// the rest of the Rust code, such that it can be reconstituted if necessary.
pub struct ParsedFile(Vec<Segment>);

#[allow(clippy::large_enum_variant)]
enum Segment {
    Autocxx(IncludeCppEngine),
    Cxx(CxxBridge),
    Mod(Vec<Segment>, (Brace, ItemMod)),
    Other(Item),
}

pub trait CppBuildable {
    fn generate_h_and_cxx(
        &self,
        cpp_codegen_options: &CppCodegenOptions,
    ) -> Result<GeneratedCpp, cxx_gen::Error>;
}

impl ParsedFile {
    /// Get all the autocxx `include_cpp` macros found in this file.
    pub fn get_autocxxes(&self) -> impl Iterator<Item = &IncludeCppEngine> {
        fn do_get_autocxxes(segments: &[Segment]) -> impl Iterator<Item = &IncludeCppEngine> {
            segments
                .iter()
                .flat_map(|s| -> Box<dyn Iterator<Item = &IncludeCppEngine>> {
                    match s {
                        Segment::Autocxx(includecpp) => Box::new(std::iter::once(includecpp)),
                        Segment::Mod(segments, _) => Box::new(do_get_autocxxes(segments)),
                        _ => Box::new(std::iter::empty()),
                    }
                })
        }

        do_get_autocxxes(&self.0)
    }

    /// Get all the areas of Rust code which need to be built for these bindings.
    /// A shortcut for `get_autocxxes()` then calling `get_rs_output` on each.
    pub fn get_rs_outputs(&self) -> impl Iterator<Item = RsOutput> {
        self.get_autocxxes().map(|autocxx| autocxx.get_rs_output())
    }

    /// Get all items which can result in C++ code
    pub fn get_cpp_buildables(&self) -> impl Iterator<Item = &dyn CppBuildable> {
        fn do_get_cpp_buildables(segments: &[Segment]) -> impl Iterator<Item = &dyn CppBuildable> {
            segments
                .iter()
                .flat_map(|s| -> Box<dyn Iterator<Item = &dyn CppBuildable>> {
                    match s {
                        Segment::Autocxx(includecpp) => {
                            Box::new(std::iter::once(includecpp as &dyn CppBuildable))
                        }
                        Segment::Cxx(cxxbridge) => {
                            Box::new(std::iter::once(cxxbridge as &dyn CppBuildable))
                        }
                        Segment::Mod(segments, _) => Box::new(do_get_cpp_buildables(segments)),
                        _ => Box::new(std::iter::empty()),
                    }
                })
        }

        do_get_cpp_buildables(&self.0)
    }

    fn get_autocxxes_mut(&mut self) -> impl Iterator<Item = &mut IncludeCppEngine> {
        fn do_get_autocxxes_mut(
            segments: &mut [Segment],
        ) -> impl Iterator<Item = &mut IncludeCppEngine> {
            segments
                .iter_mut()
                .flat_map(|s| -> Box<dyn Iterator<Item = &mut IncludeCppEngine>> {
                    match s {
                        Segment::Autocxx(includecpp) => Box::new(std::iter::once(includecpp)),
                        Segment::Mod(segments, _) => Box::new(do_get_autocxxes_mut(segments)),
                        _ => Box::new(std::iter::empty()),
                    }
                })
        }

        do_get_autocxxes_mut(&mut self.0)
    }

    /// Determines the include dirs that were set for each include_cpp, so they can be
    /// used as input to a `cc::Build`.
    #[cfg(any(test, feature = "build"))]
    pub(crate) fn include_dirs(&self) -> impl Iterator<Item = &PathBuf> {
        fn do_get_include_dirs(segments: &[Segment]) -> impl Iterator<Item = &PathBuf> {
            segments
                .iter()
                .flat_map(|s| -> Box<dyn Iterator<Item = &PathBuf>> {
                    match s {
                        Segment::Autocxx(includecpp) => Box::new(includecpp.include_dirs()),
                        Segment::Mod(segments, _) => Box::new(do_get_include_dirs(segments)),
                        _ => Box::new(std::iter::empty()),
                    }
                })
        }

        do_get_include_dirs(&self.0)
    }

    pub fn resolve_all(
        &mut self,
        autocxx_inc: Vec<PathBuf>,
        extra_clang_args: &[&str],
        dep_recorder: Option<Box<dyn RebuildDependencyRecorder>>,
        cpp_codegen_options: &CppCodegenOptions,
    ) -> Result<(), ParseError> {
        let mut mods_found = HashSet::new();
        let inner_dep_recorder: Option<Rc<dyn RebuildDependencyRecorder>> =
            dep_recorder.map(Rc::from);
        for include_cpp in self.get_autocxxes_mut() {
            #[allow(clippy::manual_map)] // because of dyn shenanigans
            let dep_recorder: Option<Box<dyn RebuildDependencyRecorder>> = match &inner_dep_recorder
            {
                None => None,
                Some(inner_dep_recorder) => Some(Box::new(CompositeDepRecorder::new(
                    inner_dep_recorder.clone(),
                ))),
            };
            if !mods_found.insert(include_cpp.get_mod_name()) {
                return Err(ParseError::ConflictingModNames);
            }
            include_cpp
                .generate(
                    autocxx_inc.clone(),
                    extra_clang_args,
                    dep_recorder,
                    cpp_codegen_options,
                )
                .map_err(ParseError::AutocxxCodegenError)?
        }
        Ok(())
    }
}

/// Shenanigans required to share the same RebuildDependencyRecorder
/// with all of the include_cpp instances in this one file.
#[derive(Debug, Clone)]
struct CompositeDepRecorder(Rc<dyn RebuildDependencyRecorder>);

impl CompositeDepRecorder {
    fn new(inner: Rc<dyn RebuildDependencyRecorder>) -> Self {
        CompositeDepRecorder(inner)
    }
}

impl UnwindSafe for CompositeDepRecorder {}

impl RebuildDependencyRecorder for CompositeDepRecorder {
    fn record_header_file_dependency(&self, filename: &str) {
        self.0.record_header_file_dependency(filename);
    }
}
