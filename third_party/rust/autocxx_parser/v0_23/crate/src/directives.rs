// Copyright 2022 Google LLC
//
// Licensed under the Apache License, Version 2.0 <LICENSE-APACHE or
// https://www.apache.org/licenses/LICENSE-2.0> or the MIT license
// <LICENSE-MIT or https://opensource.org/licenses/MIT>, at your
// option. This file may not be copied, modified, or distributed
// except according to those terms.

use indexmap::map::IndexMap as HashMap;

use once_cell::sync::OnceCell;
use proc_macro2::Span;
use proc_macro2::{Ident, TokenStream};
use quote::{quote, ToTokens};
use syn::parse::ParseStream;

use crate::config::{Allowlist, AllowlistErr};
use crate::directive_names::{EXTERN_RUST_FUN, EXTERN_RUST_TYPE, SUBCLASS};
use crate::{AllowlistEntry, IncludeCppConfig};
use crate::{ParseResult, RustFun, RustPath};

pub(crate) struct DirectivesMap {
    pub(crate) need_hexathorpe: HashMap<String, Box<dyn Directive>>,
    pub(crate) need_exclamation: HashMap<String, Box<dyn Directive>>,
}

static DIRECTIVES: OnceCell<DirectivesMap> = OnceCell::new();

pub(crate) fn get_directives() -> &'static DirectivesMap {
    DIRECTIVES.get_or_init(|| {
        let mut need_hexathorpe: HashMap<String, Box<dyn Directive>> = HashMap::new();
        need_hexathorpe.insert("include".into(), Box::new(Inclusion));
        let mut need_exclamation: HashMap<String, Box<dyn Directive>> = HashMap::new();
        need_exclamation.insert("generate".into(), Box::new(Generate(false)));
        need_exclamation.insert("generate_pod".into(), Box::new(Generate(true)));
        need_exclamation.insert("generate_ns".into(), Box::new(GenerateNs));
        need_exclamation.insert("generate_all".into(), Box::new(GenerateAll));
        need_exclamation.insert("safety".into(), Box::new(Safety));
        need_exclamation.insert(
            "pod".into(),
            Box::new(StringList(
                |config| &mut config.pod_requests,
                |config| &config.pod_requests,
            )),
        );
        need_exclamation.insert(
            "block".into(),
            Box::new(StringList(
                |config| &mut config.blocklist,
                |config| &config.blocklist,
            )),
        );
        need_exclamation.insert(
            "block_constructors".into(),
            Box::new(StringList(
                |config| &mut config.constructor_blocklist,
                |config| &config.constructor_blocklist,
            )),
        );
        need_exclamation.insert(
            "instantiable".into(),
            Box::new(StringList(
                |config| &mut config.instantiable,
                |config| &config.instantiable,
            )),
        );
        need_exclamation.insert(
            "parse_only".into(),
            Box::new(BoolFlag(
                |config| &mut config.parse_only,
                |config| &config.parse_only,
            )),
        );
        need_exclamation.insert(
            "exclude_impls".into(),
            Box::new(BoolFlag(
                |config| &mut config.exclude_impls,
                |config| &config.exclude_impls,
            )),
        );
        need_exclamation.insert(
            "exclude_utilities".into(),
            Box::new(BoolFlag(
                |config| &mut config.exclude_utilities,
                |config| &config.exclude_utilities,
            )),
        );
        need_exclamation.insert("name".into(), Box::new(ModName));
        need_exclamation.insert("concrete".into(), Box::new(Concrete));
        need_exclamation.insert("rust_type".into(), Box::new(RustType { output: false }));
        need_exclamation.insert(EXTERN_RUST_TYPE.into(), Box::new(RustType { output: true }));
        need_exclamation.insert(SUBCLASS.into(), Box::new(Subclass));
        need_exclamation.insert(EXTERN_RUST_FUN.into(), Box::new(ExternRustFun));
        need_exclamation.insert(
            "extern_cpp_type".into(),
            Box::new(ExternCppType { opaque: false }),
        );
        need_exclamation.insert(
            "extern_cpp_opaque_type".into(),
            Box::new(ExternCppType { opaque: true }),
        );

        DirectivesMap {
            need_hexathorpe,
            need_exclamation,
        }
    })
}

/// Trait for handling an `include_cpp!` configuration directive.
pub(crate) trait Directive: Send + Sync {
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        span: &Span,
    ) -> ParseResult<()>;
    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a>;
}

struct Inclusion;

impl Directive for Inclusion {
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        _span: &Span,
    ) -> ParseResult<()> {
        let hdr: syn::LitStr = args.parse()?;
        config.inclusions.push(hdr.value());
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        Box::new(config.inclusions.iter().map(|val| quote! { #val }))
    }
}

/// Directive for either `generate!` (false) or `generate_pod!` (true).
struct Generate(bool);

impl Directive for Generate {
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        span: &Span,
    ) -> ParseResult<()> {
        let generate: syn::LitStr = args.parse()?;
        config
            .allowlist
            .push(AllowlistEntry::Item(generate.value()))
            .map_err(|e| allowlist_err_to_syn_err(e, span))?;
        if self.0 {
            config.pod_requests.push(generate.value());
        }
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        match &config.allowlist {
            Allowlist::Specific(items) if !self.0 => Box::new(
                items
                    .iter()
                    .flat_map(|i| match i {
                        AllowlistEntry::Item(s) => Some(s),
                        _ => None,
                    })
                    .map(|s| quote! { #s }),
            ),
            Allowlist::Unspecified(_) => panic!("Allowlist mode not yet determined"),
            _ => Box::new(std::iter::empty()),
        }
    }
}

struct GenerateNs;

impl Directive for GenerateNs {
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        span: &Span,
    ) -> ParseResult<()> {
        let generate: syn::LitStr = args.parse()?;
        config
            .allowlist
            .push(AllowlistEntry::Namespace(generate.value()))
            .map_err(|e| allowlist_err_to_syn_err(e, span))?;
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        match &config.allowlist {
            Allowlist::Specific(items) => Box::new(
                items
                    .iter()
                    .flat_map(|i| match i {
                        AllowlistEntry::Namespace(s) => Some(s),
                        _ => None,
                    })
                    .map(|s| quote! { #s }),
            ),
            Allowlist::Unspecified(_) => panic!("Allowlist mode not yet determined"),
            _ => Box::new(std::iter::empty()),
        }
    }
}

struct GenerateAll;

impl Directive for GenerateAll {
    fn parse(
        &self,
        _args: ParseStream,
        config: &mut IncludeCppConfig,
        span: &Span,
    ) -> ParseResult<()> {
        config
            .allowlist
            .set_all()
            .map_err(|e| allowlist_err_to_syn_err(e, span))?;
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        match &config.allowlist {
            Allowlist::All => Box::new(std::iter::once(TokenStream::new())),
            Allowlist::Unspecified(_) => panic!("Allowlist mode not yet determined"),
            _ => Box::new(std::iter::empty()),
        }
    }
}

struct Safety;

impl Directive for Safety {
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        _ident_span: &Span,
    ) -> ParseResult<()> {
        config.unsafe_policy = args.parse()?;
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        let policy = &config.unsafe_policy;
        match config.unsafe_policy {
            crate::UnsafePolicy::AllFunctionsUnsafe => Box::new(std::iter::empty()),
            _ => Box::new(std::iter::once(policy.to_token_stream())),
        }
    }
}

fn allowlist_err_to_syn_err(err: AllowlistErr, span: &Span) -> syn::Error {
    syn::Error::new(*span, format!("{}", err))
}

struct StringList<SET, GET>(SET, GET)
where
    SET: Fn(&mut IncludeCppConfig) -> &mut Vec<String>,
    GET: Fn(&IncludeCppConfig) -> &Vec<String>;

impl<SET, GET> Directive for StringList<SET, GET>
where
    SET: Fn(&mut IncludeCppConfig) -> &mut Vec<String> + Sync + Send,
    GET: Fn(&IncludeCppConfig) -> &Vec<String> + Sync + Send,
{
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        _ident_span: &Span,
    ) -> ParseResult<()> {
        let val: syn::LitStr = args.parse()?;
        self.0(config).push(val.value());
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        Box::new(self.1(config).iter().map(|val| {
            quote! {
                #val
            }
        }))
    }
}

struct BoolFlag<SET, GET>(SET, GET)
where
    SET: Fn(&mut IncludeCppConfig) -> &mut bool,
    GET: Fn(&IncludeCppConfig) -> &bool;

impl<SET, GET> Directive for BoolFlag<SET, GET>
where
    SET: Fn(&mut IncludeCppConfig) -> &mut bool + Sync + Send,
    GET: Fn(&IncludeCppConfig) -> &bool + Sync + Send,
{
    fn parse(
        &self,
        _args: ParseStream,
        config: &mut IncludeCppConfig,
        _ident_span: &Span,
    ) -> ParseResult<()> {
        *self.0(config) = true;
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        if *self.1(config) {
            Box::new(std::iter::once(quote! {}))
        } else {
            Box::new(std::iter::empty())
        }
    }
}

struct ModName;

impl Directive for ModName {
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        _ident_span: &Span,
    ) -> ParseResult<()> {
        let id: Ident = args.parse()?;
        config.mod_name = Some(id);
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        match &config.mod_name {
            None => Box::new(std::iter::empty()),
            Some(id) => Box::new(std::iter::once(quote! { #id })),
        }
    }
}

struct Concrete;

impl Directive for Concrete {
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        _ident_span: &Span,
    ) -> ParseResult<()> {
        let definition: syn::LitStr = args.parse()?;
        args.parse::<syn::token::Comma>()?;
        let rust_id: syn::Ident = args.parse()?;
        config.concretes.0.insert(definition.value(), rust_id);
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        Box::new(config.concretes.0.iter().map(|(k, v)| {
            quote! {
                #k,#v
            }
        }))
    }
}

struct RustType {
    output: bool,
}

impl Directive for RustType {
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        _ident_span: &Span,
    ) -> ParseResult<()> {
        let id: Ident = args.parse()?;
        config.rust_types.push(RustPath::new_from_ident(id));
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        if self.output {
            Box::new(config.rust_types.iter().map(|rp| rp.to_token_stream()))
        } else {
            Box::new(std::iter::empty())
        }
    }
}

struct Subclass;

impl Directive for Subclass {
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        _ident_span: &Span,
    ) -> ParseResult<()> {
        let superclass: syn::LitStr = args.parse()?;
        args.parse::<syn::token::Comma>()?;
        let subclass: syn::Ident = args.parse()?;
        config.subclasses.push(crate::config::Subclass {
            superclass: superclass.value(),
            subclass,
        });
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        Box::new(config.subclasses.iter().map(|sc| {
            let superclass = &sc.superclass;
            let subclass = &sc.subclass;
            quote! {
                #superclass,#subclass
            }
        }))
    }
}

struct ExternRustFun;

impl Directive for ExternRustFun {
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        _ident_span: &Span,
    ) -> ParseResult<()> {
        let path: RustPath = args.parse()?;
        args.parse::<syn::token::Comma>()?;
        let sig: syn::Signature = args.parse()?;
        config.extern_rust_funs.push(RustFun {
            path,
            sig,
            has_receiver: false,
        });
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        Box::new(config.extern_rust_funs.iter().map(|erf| {
            let p = &erf.path;
            let s = &erf.sig;
            quote! { #p,#s }
        }))
    }
}

struct ExternCppType {
    opaque: bool,
}

impl Directive for ExternCppType {
    fn parse(
        &self,
        args: ParseStream,
        config: &mut IncludeCppConfig,
        _ident_span: &Span,
    ) -> ParseResult<()> {
        let definition: syn::LitStr = args.parse()?;
        args.parse::<syn::token::Comma>()?;
        let rust_path: syn::TypePath = args.parse()?;
        config.externs.0.insert(
            definition.value(),
            crate::config::ExternCppType {
                rust_path,
                opaque: self.opaque,
            },
        );
        Ok(())
    }

    fn output<'a>(
        &self,
        config: &'a IncludeCppConfig,
    ) -> Box<dyn Iterator<Item = TokenStream> + 'a> {
        let opaque_needed = self.opaque;
        Box::new(
            config
                .externs
                .0
                .iter()
                .filter_map(move |(definition, details)| {
                    if details.opaque == opaque_needed {
                        let rust_path = &details.rust_path;
                        Some(quote! {
                            #definition, #rust_path
                        })
                    } else {
                        None
                    }
                }),
        )
    }
}
