use std::ops::Deref;

use proc_macro2::{Ident, Span, TokenStream};
use quote::{quote, quote_spanned};
use syn::parse::Parser;
use syn::punctuated::Punctuated;
use syn::{Expr, ExprBlock};
use syn::{parse::Error, spanned::Spanned};

use crate::mutator::Mutator;
use crate::struct_info::StructInfo;
use crate::util::{ApplyMeta, AttrArg, expr_to_lit_string, ident_to_type, path_to_single_string, strip_raw_ident_prefix};

#[derive(Debug)]
pub struct FieldInfo<'a> {
    pub ordinal: usize,
    pub name: &'a syn::Ident,
    pub generic_ident: syn::Ident,
    pub ty: &'a syn::Type,
    pub builder_attr: FieldBuilderAttr<'a>,
}

impl<'a> FieldInfo<'a> {
    pub fn new(ordinal: usize, field: &'a syn::Field, field_defaults: FieldBuilderAttr<'a>) -> Result<FieldInfo<'a>, Error> {
        if let Some(ref name) = field.ident {
            FieldInfo {
                ordinal,
                name,
                generic_ident: syn::Ident::new(&format!("__{}", strip_raw_ident_prefix(name.to_string())), Span::call_site()),
                ty: &field.ty,
                builder_attr: field_defaults.with(name, &field.attrs)?,
            }
            .post_process()
        } else {
            Err(Error::new(field.span(), "Nameless field in struct"))
        }
    }

    pub fn generic_ty_param(&self) -> syn::GenericParam {
        syn::GenericParam::Type(self.generic_ident.clone().into())
    }

    pub fn type_ident(&self) -> syn::Type {
        ident_to_type(self.generic_ident.clone())
    }

    pub fn tuplized_type_ty_param(&self) -> syn::Type {
        let mut types = Punctuated::default();
        types.push(self.ty.clone());
        types.push_punct(Default::default());
        syn::TypeTuple {
            paren_token: Default::default(),
            elems: types,
        }
        .into()
    }

    pub fn type_from_inside_option(&self) -> Option<&syn::Type> {
        let typ = if let syn::Type::Group(type_group) = self.ty {
            type_group.elem.deref()
        } else {
            self.ty
        };

        let path = if let syn::Type::Path(type_path) = typ {
            if type_path.qself.is_some() {
                return None;
            }
            &type_path.path
        } else {
            return None;
        };
        let segment = path.segments.last()?;
        if segment.ident != "Option" {
            return None;
        }
        let generic_params = if let syn::PathArguments::AngleBracketed(generic_params) = &segment.arguments {
            generic_params
        } else {
            return None;
        };
        if let syn::GenericArgument::Type(ty) = generic_params.args.first()? {
            Some(ty)
        } else {
            None
        }
    }

    pub fn setter_method_name(&self) -> Ident {
        let name = strip_raw_ident_prefix(self.name.to_string());

        if let (Some(prefix), Some(suffix)) = (&self.builder_attr.setter.prefix, &self.builder_attr.setter.suffix) {
            Ident::new(&format!("{}{}{}", prefix, name, suffix), Span::call_site())
        } else if let Some(prefix) = &self.builder_attr.setter.prefix {
            Ident::new(&format!("{}{}", prefix, name), Span::call_site())
        } else if let Some(suffix) = &self.builder_attr.setter.suffix {
            Ident::new(&format!("{}{}", name, suffix), Span::call_site())
        } else {
            self.name.clone()
        }
    }

    fn post_process(mut self) -> Result<Self, Error> {
        if let Some(ref strip_bool) = self.builder_attr.setter.strip_bool {
            if let Some(default_span) = self.builder_attr.default.as_ref().map(Spanned::span) {
                let mut error = Error::new(
                    strip_bool.span,
                    "cannot set both strip_bool and default - default is assumed to be false",
                );
                error.combine(Error::new(default_span, "default set here"));
                return Err(error);
            }
            self.builder_attr.default = Some(syn::Expr::Lit(syn::ExprLit {
                attrs: Default::default(),
                lit: syn::Lit::Bool(syn::LitBool {
                    value: false,
                    span: strip_bool.span,
                }),
            }));
        }
        if self.builder_attr.default.is_none()
            && let Some(default_where) = self.builder_attr.default_where.as_ref()
        {
            return Err(Error::new(
                default_where.span(),
                "default_where is not allowed without a default",
            ));
        }
        Ok(self)
    }

    pub fn maybe_mut(&self) -> TokenStream {
        if let Some(span) = self.builder_attr.mutable_during_default_resolution {
            quote_spanned!(span => mut)
        } else {
            quote!()
        }
    }

    pub fn gen_next_field_default_trait_impl(&self, struct_info: &StructInfo) -> syn::Result<Option<TokenStream>> {
        if self.builder_attr.setter.skip.is_some() {
            return Ok(None);
        }
        let Some(default_expr) = self.builder_attr.default.as_ref() else {
            return Ok(None);
        };

        let crate_module_path = &struct_info.builder_attr.crate_module_path;
        let struct_name = struct_info.name;
        let (impl_generics, ty_generics, where_clause) = struct_info.generics.split_for_impl();
        let field_type = self.ty;

        let (dep_types, dep_names): (Vec<_>, Vec<_>) = struct_info
            .fields
            .iter()
            .take(self.ordinal)
            .map(|dep_field| {
                let dep_type = dep_field.ty;
                let dep_mut = dep_field.maybe_mut();
                (quote!(&#dep_mut #dep_type), dep_field.name)
            })
            .unzip();

        let where_clause_storage;
        let where_clause_for_default = if let Some(default_where) = self.builder_attr.default_where.as_ref() {
            let mut predicates: Punctuated<_, _> = Default::default();
            if let Some(where_clause) = where_clause {
                predicates.extend(where_clause.predicates.iter().cloned());
            }
            predicates.extend(default_where.iter().cloned());
            where_clause_storage = syn::WhereClause {
                where_token: Default::default(),
                predicates,
            };
            Some(&where_clause_storage)
        } else {
            where_clause
        };

        Ok(Some(quote! {
            #[allow(clippy::ref_option_ref)]
            #[automatically_derived]
            impl #impl_generics #crate_module_path::NextFieldDefault<(#(#dep_types,)* (#field_type,),)> for #struct_name #ty_generics #where_clause {
                type Output = #field_type;

                fn resolve((.., (input,),): (#(#dep_types,)* (#field_type,),)) -> Self::Output {
                    input
                }
            }

            #[allow(clippy::ref_option_ref)]
            #[automatically_derived]
            impl #impl_generics #crate_module_path::NextFieldDefault<(#(#dep_types,)* (),)> for #struct_name #ty_generics #where_clause_for_default {
                type Output = #field_type;

                fn resolve((#(#dep_names,)* (),): (#(#dep_types,)* (),)) -> Self::Output {
                    #default_expr
                }
            }
        }))
    }
}

#[derive(Debug, Default, Clone)]
pub struct FieldBuilderAttr<'a> {
    pub default: Option<syn::Expr>,
    pub default_where: Option<Punctuated<syn::WherePredicate, syn::token::Comma>>,
    pub via_mutators: Option<ViaMutators>,
    pub deprecated: Option<&'a syn::Attribute>,
    pub doc_comments: Vec<&'a syn::Expr>,
    pub setter: SetterSettings,
    /// Functions that are able to mutate fields in the builder that are already set
    pub mutators: Vec<Mutator>,
    pub mutable_during_default_resolution: Option<Span>,
}

#[derive(Debug, Default, Clone)]
pub struct SetterSettings {
    pub doc: Option<syn::Expr>,
    pub skip: Option<Span>,
    pub auto_into: Option<Span>,
    pub strip_option: Option<Strip>,
    pub strip_bool: Option<Strip>,
    pub transform: Option<Transform>,
    pub prefix: Option<String>,
    pub suffix: Option<String>,
}

impl<'a> FieldBuilderAttr<'a> {
    pub fn with(mut self, name: &Ident, attrs: &'a [syn::Attribute]) -> Result<Self, Error> {
        for attr in attrs {
            let list = match &attr.meta {
                syn::Meta::List(list) => {
                    let Some(path) = path_to_single_string(&list.path) else {
                        continue;
                    };

                    if path == "deprecated" {
                        self.deprecated = Some(attr);
                        continue;
                    }

                    if path != "builder" {
                        continue;
                    }

                    list
                }
                syn::Meta::NameValue(syn::MetaNameValue { path, value, .. }) => {
                    match path_to_single_string(path).as_deref() {
                        Some("deprecated") => self.deprecated = Some(attr),
                        Some("doc") => self.doc_comments.push(value),
                        _ => continue,
                    }

                    continue;
                }
                syn::Meta::Path(path) => {
                    match path_to_single_string(path).as_deref() {
                        Some("deprecated") => self.deprecated = Some(attr),
                        _ => continue,
                    }

                    continue;
                }
            };

            self.apply_subsections(list)?;
        }

        for mutator in self.mutators.iter_mut() {
            mutator.required_fields.insert(name.clone());
        }

        self.inter_fields_conflicts()?;

        Ok(self)
    }

    fn inter_fields_conflicts(&self) -> Result<(), Error> {
        if let (Some(skip), None) = (&self.setter.skip, &self.default) {
            return Err(Error::new(
                *skip,
                "#[builder(skip)] must be accompanied by default or default_code",
            ));
        }

        let conflicting_transformations = [
            ("transform", self.setter.transform.as_ref().map(|t| &t.span)),
            ("strip_option", self.setter.strip_option.as_ref().map(|s| &s.span)),
            ("strip_bool", self.setter.strip_bool.as_ref().map(|s| &s.span)),
        ];
        let mut conflicting_transformations = conflicting_transformations
            .iter()
            .filter_map(|(caption, span)| span.map(|span| (caption, span)))
            .collect::<Vec<_>>();

        if 1 < conflicting_transformations.len() {
            let (first_caption, first_span) = conflicting_transformations.pop().unwrap();
            let conflicting_captions = conflicting_transformations
                .iter()
                .map(|(caption, _)| **caption)
                .collect::<Vec<_>>();
            let mut error = Error::new(
                *first_span,
                format_args!("{} conflicts with {}", first_caption, conflicting_captions.join(", ")),
            );
            for (caption, span) in conflicting_transformations {
                error.combine(Error::new(*span, format_args!("{} set here", caption)));
            }
            return Err(error);
        }
        Ok(())
    }
}

impl ApplyMeta for FieldBuilderAttr<'_> {
    fn apply_meta(&mut self, expr: AttrArg) -> Result<(), Error> {
        match expr.name().to_string().as_str() {
            "default" => match expr {
                AttrArg::Flag(ident) => {
                    self.default =
                        Some(syn::parse2(quote_spanned!(ident.span() => ::core::default::Default::default())).unwrap());
                    Ok(())
                }
                AttrArg::KeyValue(key_value) => {
                    self.default = Some(key_value.parse_value()?);
                    Ok(())
                }
                AttrArg::Not { .. } => {
                    self.default = None;
                    Ok(())
                }
                AttrArg::Sub(_) => Err(expr.incorrect_type()),
                AttrArg::Fn(_) => Err(expr.incorrect_type()),
            },
            "default_where" => {
                let sub_attr = expr.sub_attr()?;
                let span = sub_attr.args.span();
                self.default_where = Some(
                    Punctuated::parse_terminated
                        .parse2(sub_attr.args)
                        .map_err(|e| Error::new(span, format!("{e}")))?,
                );
                Ok(())
            }
            "default_code" => {
                use std::str::FromStr;

                let code = expr.key_value()?.parse_value::<syn::LitStr>()?;
                let tokenized_code = TokenStream::from_str(&code.value())?;
                self.default = Some(syn::parse2(tokenized_code).map_err(|e| Error::new_spanned(code, format!("{}", e)))?);

                Ok(())
            }
            "setter" => self.setter.apply_sub_attr(expr.sub_attr()?),
            "mutable_during_default_resolution" => expr.apply_flag_to_field(
                &mut self.mutable_during_default_resolution,
                "made mutable during default resolution",
            ),
            "via_mutators" => {
                match expr {
                    AttrArg::Flag(ident) => {
                        self.via_mutators = Some(ViaMutators {
                            span: ident.span(),
                            init: syn::parse2(quote_spanned!(ident.span() => ::core::default::Default::default())).unwrap(),
                        });
                    }
                    AttrArg::KeyValue(key_value) => {
                        self.via_mutators = Some(ViaMutators {
                            span: key_value.span(),
                            init: key_value.parse_value()?,
                        });
                    }
                    AttrArg::Not { .. } => {
                        self.via_mutators = None;
                    }
                    AttrArg::Sub(sub) => {
                        if let Some(via_mutators) = self.via_mutators.as_mut() {
                            if let Some(joined_span) = via_mutators.span.join(sub.span()) {
                                via_mutators.span = joined_span;
                            } else {
                                // Shouldn't happen, but whatever
                                via_mutators.span = sub.span();
                            };
                            via_mutators.apply_sub_attr(sub)?;
                        } else {
                            let mut via_mutators = ViaMutators::empty_spanned(sub.span());
                            via_mutators.apply_sub_attr(sub)?;
                            self.via_mutators = Some(via_mutators);
                        }
                    }
                    AttrArg::Fn(_) => return Err(expr.incorrect_type()),
                }
                Ok(())
            }
            "mutators" => {
                self.mutators.extend(expr.sub_attr()?.undelimited()?);
                Ok(())
            }
            _ => Err(Error::new_spanned(
                expr.name(),
                format!("Unknown parameter {:?}", expr.name().to_string()),
            )),
        }
    }
}

impl ApplyMeta for SetterSettings {
    fn apply_meta(&mut self, expr: AttrArg) -> Result<(), Error> {
        match expr.name().to_string().as_str() {
            "doc" => {
                self.doc = expr.key_value_or_not()?.map(|kv| kv.parse_value()).transpose()?;
                Ok(())
            }
            "transform" => {
                self.transform = match expr {
                    AttrArg::Fn(func) => Some(parse_transform_fn(func.span(), func)?),
                    AttrArg::KeyValue(key_value) => {
                        Some(parse_transform_closure(key_value.name.span(), key_value.parse_value()?)?)
                    }
                    AttrArg::Not { .. } => None,
                    _ => return Err(expr.incorrect_type()),
                };
                Ok(())
            }
            "prefix" => {
                self.prefix = if let Some(key_value) = expr.key_value_or_not()? {
                    Some(expr_to_lit_string(&key_value.parse_value()?)?)
                } else {
                    None
                };
                Ok(())
            }
            "suffix" => {
                self.suffix = if let Some(key_value) = expr.key_value_or_not()? {
                    Some(expr_to_lit_string(&key_value.parse_value()?)?)
                } else {
                    None
                };
                Ok(())
            }
            "skip" => expr.apply_flag_to_field(&mut self.skip, "skipped"),
            "into" => expr.apply_flag_to_field(&mut self.auto_into, "calling into() on the argument"),
            "strip_option" => {
                expr.apply_potentialy_empty_sub_to_field(&mut self.strip_option, "putting the argument in Some(...)", Strip::new)
            }
            "strip_bool" => expr.apply_potentialy_empty_sub_to_field(
                &mut self.strip_bool,
                "zero arguments setter, sets the field to true",
                Strip::new,
            ),
            _ => Err(Error::new_spanned(
                expr.name(),
                format!("Unknown parameter {:?}", expr.name().to_string()),
            )),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Strip {
    pub fallback: Option<syn::Ident>,
    pub fallback_prefix: Option<String>,
    pub fallback_suffix: Option<String>,
    pub ignore_invalid: bool,
    span: Span,
}

impl Strip {
    fn new(span: Span) -> Self {
        Self {
            fallback: None,
            fallback_prefix: None,
            fallback_suffix: None,
            ignore_invalid: false,
            span,
        }
    }
}

impl ApplyMeta for Strip {
    fn apply_meta(&mut self, expr: AttrArg) -> Result<(), Error> {
        match expr.name().to_string().as_str() {
            "fallback" => {
                if self.fallback.is_some() {
                    return Err(Error::new_spanned(
                        expr.name(),
                        format!("Duplicate fallback parameter {:?}", expr.name().to_string()),
                    ));
                }

                let ident: syn::Ident = expr.key_value().map(|kv| kv.parse_value())??;
                self.fallback = Some(ident);
                Ok(())
            }
            "fallback_prefix" => {
                if self.fallback_prefix.is_some() {
                    return Err(Error::new_spanned(
                        expr.name(),
                        format!("Duplicate fallback_prefix parameter {:?}", expr.name().to_string()),
                    ));
                }

                self.fallback_prefix = Some(expr.key_value()?.parse_value::<syn::LitStr>()?.value());
                Ok(())
            }
            "fallback_suffix" => {
                if self.fallback_suffix.is_some() {
                    return Err(Error::new_spanned(
                        expr.name(),
                        format!("Duplicate fallback_suffix parameter {:?}", expr.name().to_string()),
                    ));
                }

                self.fallback_suffix = Some(expr.key_value()?.parse_value::<syn::LitStr>()?.value());
                Ok(())
            }
            "ignore_invalid" => {
                if self.ignore_invalid {
                    return Err(Error::new_spanned(
                        expr.name(),
                        format!("Duplicate ignore_invalid parameter {:?}", expr.name().to_string()),
                    ));
                }

                expr.flag()?;
                self.ignore_invalid = true;
                Ok(())
            }
            _ => Err(Error::new_spanned(
                expr.name(),
                format!("Unknown parameter {:?}", expr.name().to_string()),
            )),
        }
    }
}

#[derive(Debug, Clone)]
pub struct Transform {
    pub params: Vec<(syn::Pat, syn::Type)>,
    pub body: syn::Expr,
    pub generics: Option<syn::Generics>,
    pub return_type: syn::ReturnType,
    span: Span,
}

fn parse_transform_fn(span: Span, func: syn::ItemFn) -> Result<Transform, Error> {
    if let Some(kw) = &func.sig.asyncness {
        return Err(Error::new(kw.span, "Transform function cannot be async"));
    }

    let params = func
        .sig
        .inputs
        .into_iter()
        .map(|input| match input {
            syn::FnArg::Typed(pat_type) => Ok((*pat_type.pat, *pat_type.ty)),
            syn::FnArg::Receiver(_) => Err(Error::new_spanned(input, "Transform function cannot have self parameter")),
        })
        .collect::<Result<Vec<_>, _>>()?;

    let body = Expr::Block(ExprBlock {
        attrs: Vec::new(),
        label: None,
        block: *func.block,
    });

    Ok(Transform {
        params,
        body,
        span,
        generics: Some(func.sig.generics),
        return_type: func.sig.output,
    })
}

fn parse_transform_closure(span: Span, expr: syn::Expr) -> Result<Transform, Error> {
    let closure = match expr {
        syn::Expr::Closure(closure) => closure,
        _ => return Err(Error::new_spanned(expr, "Expected closure")),
    };

    if let Some(kw) = &closure.asyncness {
        return Err(Error::new(kw.span, "Transform closure cannot be async"));
    }
    if let Some(kw) = &closure.capture {
        return Err(Error::new(kw.span, "Transform closure cannot be move"));
    }

    let params = closure
        .inputs
        .into_iter()
        .map(|input| match input {
            syn::Pat::Type(pat_type) => Ok((*pat_type.pat, *pat_type.ty)),
            _ => Err(Error::new_spanned(input, "Transform closure must explicitly declare types")),
        })
        .collect::<Result<Vec<_>, _>>()?;

    Ok(Transform {
        params,
        body: *closure.body,
        span,
        generics: None,
        return_type: closure.output,
    })
}

#[derive(Debug, Clone)]
pub struct ViaMutators {
    pub span: Span,
    pub init: syn::Expr,
}

impl ViaMutators {
    fn empty_spanned(span: Span) -> Self {
        Self {
            span,
            init: syn::parse2(quote_spanned!(span => ::core::default::Default::default())).unwrap(),
        }
    }
}

impl ApplyMeta for ViaMutators {
    fn apply_meta(&mut self, expr: AttrArg) -> Result<(), Error> {
        match expr.name().to_string().as_str() {
            "init" => {
                self.init = expr.key_value()?.parse_value()?;
                Ok(())
            }
            _ => Err(Error::new_spanned(
                expr.name(),
                format!("Unknown parameter {:?}", expr.name().to_string()),
            )),
        }
    }
}
