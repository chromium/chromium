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

use crate::ast_discoverer::Discoveries;
use crate::CppCodegenOptions;
use crate::{
    cxxbridge::CxxBridge, Error as EngineError, GeneratedCpp, IncludeCppEngine,
    RebuildDependencyRecorder,
};
use autocxx_parser::directives::SUBCLASS;
use autocxx_parser::{Subclass, SubclassAttrs};
use proc_macro2::{Span, TokenStream};
use quote::ToTokens;
use std::{collections::HashSet, fmt::Display, io::Read, path::PathBuf};
use std::{panic::UnwindSafe, path::Path, rc::Rc};
use syn::{Item, LitStr};

/// Errors which may occur when parsing a Rust source file to discover
/// and interpret include_cxx macros.
#[derive(Debug)]
pub enum ParseError {
    /// Unable to open the source file
    FileOpen(std::io::Error),
    /// The .rs file couldn't be read.
    FileRead(std::io::Error),
    /// The .rs file couldn't be parsed.
    Syntax(syn::Error),
    /// The include CPP macro could not be expanded into
    /// Rust bindings to C++, because of some problem during the conversion
    /// process. This could be anything from a C++ parsing error to some
    /// C++ feature that autocxx can't yet handle and isn't able to skip
    /// over. It could also cover errors in your syntax of the `include_cpp`
    /// macro or the directives inside.
    AutocxxCodegenError(EngineError),
    /// There are two or more `include_cpp` macros with the same
    /// mod name.
    ConflictingModNames,
    ZeroModsForDynamicDiscovery,
    MultipleModsForDynamicDiscovery,
    DiscoveredRustItemsWhenNotInAutoDiscover,
}

impl Display for ParseError {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            ParseError::FileOpen(err) => write!(f, "Unable to open file: {}", err)?,
            ParseError::FileRead(err) => write!(f, "Unable to read file: {}", err)?,
            ParseError::Syntax(err) => write!(f, "Syntax error parsing Rust file: {}", err)?,
            ParseError::AutocxxCodegenError(err) =>
                write!(f, "Unable to parse include_cpp! macro: {}", err)?,
            ParseError::ConflictingModNames =>
                write!(f, "There are two or more include_cpp! macros with the same output mod name. Use name!")?,
            ParseError::ZeroModsForDynamicDiscovery =>
                write!(f, "This file contains extra information to append to an include_cpp! but no such include_cpp! was found in this file.")?,
            ParseError::MultipleModsForDynamicDiscovery =>
                write!(f, "This file contains extra information to append to an include_cpp! but multiple such include_cpp! declarations were found in this file.")?,
            ParseError::DiscoveredRustItemsWhenNotInAutoDiscover =>
                write!(f, "This file contains extra information to append to an \"extern Rust\" but auto-discover was switched off.")?,
        }
        Ok(())
    }
}

/// Parse a Rust file, and spot any include_cpp macros within it.
pub fn parse_file<P1: AsRef<Path>>(
    rs_file: P1,
    auto_allowlist: bool,
) -> Result<ParsedFile, ParseError> {
    let mut source = String::new();
    let mut file = std::fs::File::open(rs_file).map_err(ParseError::FileOpen)?;
    file.read_to_string(&mut source)
        .map_err(ParseError::FileRead)?;
    proc_macro2::fallback::force();
    let source = syn::parse_file(&source).map_err(ParseError::Syntax)?;
    parse_file_contents(source, auto_allowlist)
}

fn parse_file_contents(source: syn::File, auto_allowlist: bool) -> Result<ParsedFile, ParseError> {
    let mut results = Vec::new();
    let mut extra_superclasses = Vec::new();
    let mut discoveries = Discoveries::default();
    for item in source.items {
        results.push(match item {
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
                    crate::IncludeCppEngine::new_from_syn(mac.mac.clone())
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
            Item::Struct(ref its) if auto_allowlist => {
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
                        let args: SubclassAttrs = is_superclass_attr
                            .parse_args()
                            .map_err(ParseError::Syntax)?;
                        if let Some(superclass) = args.superclass {
                            extra_superclasses.push(Subclass {
                                superclass,
                                subclass,
                            })
                        }
                    }
                }
                discoveries.search_item(&item);
                Segment::Other(item)
            }
            _ => {
                discoveries.search_item(&item);
                Segment::Other(item)
            }
        });
    }
    if !auto_allowlist
        && (!discoveries.extern_rust_types.is_empty() || !discoveries.extern_rust_funs.is_empty())
    {
        return Err(ParseError::DiscoveredRustItemsWhenNotInAutoDiscover);
    }
    if !extra_superclasses.is_empty() || (auto_allowlist && !discoveries.is_empty()) {
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
                            .push(LitStr::new(&cpp, Span::call_site()))
                            .map_err(ParseError::Syntax)?;
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
        seg.config
            .confirm_complete(auto_allowlist)
            .map_err(ParseError::Syntax)?;
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
    Other(Item),
}

pub trait CppBuildable {
    fn generate_h_and_cxx(
        &self,
        cpp_codegen_options: &CppCodegenOptions,
    ) -> Result<GeneratedCpp, cxx_gen::Error>;
}

impl ParsedFile {
    /// Get all the autocxxes in this parsed file.
    pub fn get_rs_buildables(&self) -> impl Iterator<Item = &IncludeCppEngine> {
        self.0.iter().filter_map(|s| match s {
            Segment::Autocxx(includecpp) => Some(includecpp),
            _ => None,
        })
    }

    /// Get all items which can result in C++ code
    pub fn get_cpp_buildables(&self) -> impl Iterator<Item = &dyn CppBuildable> {
        self.0.iter().filter_map(|s| match s {
            Segment::Autocxx(includecpp) => Some(includecpp as &dyn CppBuildable),
            Segment::Cxx(cxxbridge) => Some(cxxbridge as &dyn CppBuildable),
            _ => None,
        })
    }

    fn get_autocxxes_mut(&mut self) -> impl Iterator<Item = &mut IncludeCppEngine> {
        self.0.iter_mut().filter_map(|s| match s {
            Segment::Autocxx(includecpp) => Some(includecpp),
            _ => None,
        })
    }

    pub fn include_dirs(&self) -> impl Iterator<Item = &PathBuf> {
        self.0
            .iter()
            .filter_map(|s| match s {
                Segment::Autocxx(includecpp) => Some(includecpp.include_dirs()),
                _ => None,
            })
            .flatten()
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

impl ToTokens for ParsedFile {
    fn to_tokens(&self, tokens: &mut TokenStream) {
        for seg in &self.0 {
            match seg {
                Segment::Other(item) => item.to_tokens(tokens),
                Segment::Autocxx(autocxx) => {
                    let these_tokens = autocxx.generate_rs();
                    tokens.extend(these_tokens);
                }
                Segment::Cxx(itemmod) => itemmod.to_tokens(tokens),
            }
        }
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
