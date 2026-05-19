use proc_macro2::{Ident, Span, TokenStream};
use quote::{format_ident, quote, quote_spanned, ToTokens};
use syn::{parse::Error, parse_quote, punctuated::Punctuated, GenericArgument, ItemFn, Token};

use crate::{
    builder_attr::{IntoSetting, TypeBuilderAttr},
    field_info::FieldInfo,
    mutator::Mutator,
    util::{
        empty_type, empty_type_tuple, first_visibility, modify_types_generics_hack, phantom_data_for_generics, public_visibility,
        strip_raw_ident_prefix, type_tuple,
    },
};

#[derive(Debug)]
pub struct StructInfo<'a> {
    vis: &'a syn::Visibility,
    name: &'a syn::Ident,
    generics: &'a syn::Generics,
    fields: Vec<FieldInfo<'a>>,

    builder_attr: TypeBuilderAttr<'a>,
    builder_name: syn::Ident,
}

impl<'a> StructInfo<'a> {
    fn included_fields(&self) -> impl Iterator<Item = &FieldInfo<'a>> {
        self.fields.iter().filter(|f| f.builder_attr.setter.skip.is_none())
    }
    fn setter_fields(&self) -> impl Iterator<Item = &FieldInfo<'a>> {
        self.included_fields().filter(|f| f.builder_attr.via_mutators.is_none())
    }

    fn generic_arguments(&self) -> Punctuated<GenericArgument, Token![,]> {
        self.generics
            .params
            .iter()
            .map(|generic_param| match generic_param {
                syn::GenericParam::Type(type_param) => {
                    let ident = type_param.ident.to_token_stream();
                    syn::parse2(ident).unwrap()
                }
                syn::GenericParam::Lifetime(lifetime_def) => syn::GenericArgument::Lifetime(lifetime_def.lifetime.clone()),
                syn::GenericParam::Const(const_param) => {
                    let ident = const_param.ident.to_token_stream();
                    syn::parse2(ident).unwrap()
                }
            })
            .collect()
    }

    pub fn new(ast: &'a syn::DeriveInput, fields: impl Iterator<Item = &'a syn::Field>) -> syn::Result<StructInfo<'a>> {
        let builder_attr = TypeBuilderAttr::new(&ast.attrs)?;
        let builder_name = builder_attr
            .builder_type
            .get_name()
            .map(|name| strip_raw_ident_prefix(name.to_string()))
            .unwrap_or_else(|| strip_raw_ident_prefix(format!("{}Builder", ast.ident)));
        Ok(StructInfo {
            vis: &ast.vis,
            name: &ast.ident,
            generics: &ast.generics,
            fields: fields
                .enumerate()
                .map(|(i, f)| FieldInfo::new(i, f, builder_attr.field_defaults.clone()))
                .collect::<Result<_, _>>()?,
            builder_attr,
            builder_name: syn::Ident::new(&builder_name, proc_macro2::Span::call_site()),
        })
    }

    fn builder_creation_impl(&self) -> syn::Result<TokenStream> {
        let StructInfo {
            vis,
            ref name,
            ref builder_name,
            ..
        } = *self;
        let (impl_generics, ty_generics, where_clause) = self.generics.split_for_impl();
        let init_fields_type = type_tuple(self.included_fields().map(|f| {
            if f.builder_attr.via_mutators.is_some() {
                f.tuplized_type_ty_param()
            } else {
                empty_type()
            }
        }));
        let init_fields_expr = self.included_fields().map(|f| {
            f.builder_attr.via_mutators.as_ref().map_or_else(
                || quote!(()),
                |via_mutators| {
                    let init = &via_mutators.init;
                    quote!((#init,))
                },
            )
        });
        let mut all_fields_param_type: syn::TypeParam =
            syn::Ident::new("TypedBuilderFields", proc_macro2::Span::call_site()).into();
        let all_fields_param = syn::GenericParam::Type(all_fields_param_type.clone());
        all_fields_param_type.default = Some(syn::Type::Tuple(init_fields_type.clone()));
        let b_generics = {
            let mut generics = self.generics.clone();
            generics.params.push(syn::GenericParam::Type(all_fields_param_type));
            generics
        };
        let generics_with_empty = modify_types_generics_hack(&ty_generics, |args| {
            args.push(syn::GenericArgument::Type(init_fields_type.clone().into()));
        });
        let phantom_data = phantom_data_for_generics(self.generics);

        let builder_method_name = self.builder_attr.builder_method.get_name().unwrap_or_else(|| quote!(builder));
        let builder_method_visibility = first_visibility(&[
            self.builder_attr.builder_method.vis.as_ref(),
            self.builder_attr.builder_type.vis.as_ref(),
            Some(vis),
        ]);
        let builder_method_doc = self.builder_attr.builder_method.get_doc_or(|| {
            format!(
                "
                Create a builder for building `{name}`.
                On the builder, call {setters} to set the values of the fields.
                Finally, call `.{build_method_name}()` to create the instance of `{name}`.
                ",
                name = self.name,
                build_method_name = self.build_method_name(),
                setters = {
                    let mut result = String::new();
                    let mut is_first = true;
                    for field in self.setter_fields() {
                        use std::fmt::Write;
                        if is_first {
                            is_first = false;
                        } else {
                            write!(&mut result, ", ").unwrap();
                        }
                        write!(&mut result, "`.{}(...)`", field.name).unwrap();
                        if field.builder_attr.default.is_some() {
                            write!(&mut result, "(optional)").unwrap();
                        }
                    }
                    result
                }
            )
        });

        let builder_type_visibility = first_visibility(&[self.builder_attr.builder_type.vis.as_ref(), Some(vis)]);
        let builder_type_doc = if self.builder_attr.doc {
            self.builder_attr.builder_type.get_doc_or(|| {
                format!(
                    "
                    Builder for [`{name}`] instances.

                    See [`{name}::{builder_method_name}()`] for more info.
                    ",
                    name = name,
                    builder_method_name = builder_method_name
                )
            })
        } else {
            quote!(#[doc(hidden)])
        };

        let (b_generics_impl, b_generics_ty, b_generics_where_extras_predicates) = b_generics.split_for_impl();
        let mut b_generics_where: syn::WhereClause = syn::parse2(quote! {
            where TypedBuilderFields: Clone
        })?;
        if let Some(predicates) = b_generics_where_extras_predicates {
            b_generics_where.predicates.extend(predicates.predicates.clone());
        }

        Ok(quote! {
            #[automatically_derived]
            impl #impl_generics #name #ty_generics #where_clause {
                #builder_method_doc
                #[allow(dead_code, clippy::default_trait_access)]
                #builder_method_visibility fn #builder_method_name() -> #builder_name #generics_with_empty {
                    #builder_name {
                        fields: (#(#init_fields_expr,)*),
                        phantom: ::core::default::Default::default(),
                    }
                }
            }

            #[must_use]
            #builder_type_doc
            #[allow(dead_code, non_camel_case_types, non_snake_case)]
            #builder_type_visibility struct #builder_name #b_generics #b_generics_where_extras_predicates {
                fields: #all_fields_param,
                phantom: #phantom_data,
            }

            #[automatically_derived]
            impl #b_generics_impl Clone for #builder_name #b_generics_ty #b_generics_where {
                #[allow(clippy::default_trait_access)]
                fn clone(&self) -> Self {
                    Self {
                        fields: self.fields.clone(),
                        phantom: ::core::default::Default::default(),
                    }
                }
            }
        })
    }

    fn field_impl(&self, field: &FieldInfo) -> syn::Result<TokenStream> {
        let StructInfo { ref builder_name, .. } = *self;

        let destructuring = self
            .included_fields()
            .map(|f| {
                if f.ordinal == field.ordinal {
                    quote!(())
                } else {
                    let name = f.name;
                    name.to_token_stream()
                }
            })
            .collect::<Vec<_>>();
        let reconstructing = self.included_fields().map(|f| f.name).collect::<Vec<_>>();

        let &FieldInfo {
            name: field_name,
            ty: field_type,
            ..
        } = field;
        let mut ty_generics = self.generic_arguments();
        let mut target_generics_tuple = empty_type_tuple();
        let mut ty_generics_tuple = empty_type_tuple();
        let generics = {
            let mut generics = self.generics.clone();
            for f in self.included_fields() {
                if f.ordinal == field.ordinal {
                    ty_generics_tuple.elems.push_value(empty_type());
                    target_generics_tuple.elems.push_value(f.tuplized_type_ty_param());
                } else {
                    generics.params.push(f.generic_ty_param());
                    let generic_argument: syn::Type = f.type_ident();
                    ty_generics_tuple.elems.push_value(generic_argument.clone());
                    target_generics_tuple.elems.push_value(generic_argument);
                }
                ty_generics_tuple.elems.push_punct(Default::default());
                target_generics_tuple.elems.push_punct(Default::default());
            }
            generics
        };
        let mut target_generics = ty_generics.clone();
        target_generics.push(syn::GenericArgument::Type(target_generics_tuple.into()));
        ty_generics.push(syn::GenericArgument::Type(ty_generics_tuple.into()));
        let (impl_generics, _, where_clause) = generics.split_for_impl();
        let doc = if let Some(doc) = field.builder_attr.setter.doc.as_ref() {
            Some(quote!(#[doc = #doc]))
        } else if !field.builder_attr.doc_comments.is_empty() {
            Some(
                field
                    .builder_attr
                    .doc_comments
                    .iter()
                    .map(|&line| quote!(#[doc = #line]))
                    .collect(),
            )
        } else {
            None
        };

        let deprecated = &field.builder_attr.deprecated;

        let option_was_stripped;
        let arg_type = if field.builder_attr.setter.strip_option.is_some() && field.builder_attr.setter.transform.is_none() {
            if let Some(inner_type) = field.type_from_inside_option() {
                option_was_stripped = true;
                inner_type
            } else if field
                .builder_attr
                .setter
                .strip_option
                .as_ref()
                .is_some_and(|s| s.ignore_invalid)
            {
                option_was_stripped = false;
                field_type
            } else {
                return Err(Error::new_spanned(
                    field_type,
                    "can't `strip_option` - field is not `Option<...>`",
                ));
            }
        } else {
            option_was_stripped = false;
            field_type
        };
        let (arg_type, arg_expr) = if field.builder_attr.setter.auto_into.is_some() {
            (quote!(impl ::core::convert::Into<#arg_type>), quote!(#field_name.into()))
        } else {
            (arg_type.to_token_stream(), field_name.to_token_stream())
        };

        let strip_bool_fallback = field
            .builder_attr
            .setter
            .strip_bool
            .as_ref()
            .and_then(|strip_bool| strip_bool.fallback.as_ref())
            .map(|fallback| (fallback.clone(), quote!(#field_name: #field_type), quote!(#arg_expr)));

        let strip_option_fallback = field.builder_attr.setter.strip_option.as_ref().and_then(|strip_option| {
            if let Some(ref fallback) = strip_option.fallback {
                Some((fallback.clone(), quote!(#field_name: #field_type), quote!(#arg_expr)))
            } else if strip_option.fallback_prefix.is_none() && strip_option.fallback_suffix.is_none() {
                None
            } else {
                let method = strip_raw_ident_prefix(field_name.to_string());
                let prefix = strip_option.fallback_prefix.as_deref().unwrap_or_default();
                let suffix = strip_option.fallback_suffix.as_deref().unwrap_or_default();
                let fallback_name = syn::Ident::new(&format!("{}{}{}", prefix, method, suffix), field_name.span());
                Some((fallback_name, quote!(#field_name: #field_type), quote!(#arg_expr)))
            }
        });

        let (method_generics, param_list, arg_expr, method_where_clause) = if field.builder_attr.setter.strip_bool.is_some() {
            (quote!(), quote!(), quote!(true), quote!())
        } else if let Some(transform) = &field.builder_attr.setter.transform {
            let params = transform.params.iter().map(|(pat, ty)| quote!(#pat: #ty));
            let body = &transform.body;
            let method_generics = transform.generics.as_ref().map_or(quote!(), |g| g.to_token_stream());
            let method_where_clause = transform
                .generics
                .as_ref()
                .and_then(|g| g.where_clause.as_ref())
                .map_or(quote!(), |w| w.to_token_stream());

            let body = match &transform.return_type {
                syn::ReturnType::Default => quote!({ #body }),
                syn::ReturnType::Type(_, ty) => quote!({
                    let value: #ty = { #body };
                    value
                }),
            };

            (method_generics, quote!(#(#params),*), body, method_where_clause)
        } else if option_was_stripped {
            (quote!(), quote!(#field_name: #arg_type), quote!(Some(#arg_expr)), quote!())
        } else {
            (quote!(), quote!(#field_name: #arg_type), arg_expr, quote!())
        };

        let repeated_fields_error_type_name = syn::Ident::new(
            &format!(
                "{}_Error_Repeated_field_{}",
                builder_name,
                strip_raw_ident_prefix(field_name.to_string())
            ),
            proc_macro2::Span::call_site(),
        );
        let repeated_fields_error_message = format!("Repeated field {}", field_name);

        let method_name = field.setter_method_name();

        let strip_option_fallback_method = if let Some((method_name, param_list, arg_expr)) = strip_option_fallback {
            Some(quote! {
                #deprecated
                #doc
                #[allow(clippy::used_underscore_binding, clippy::no_effect_underscore_binding)]
                pub fn #method_name #method_generics (self, #param_list) -> #builder_name <#target_generics>
                #method_where_clause
                {
                    let #field_name = (#arg_expr,);
                    let ( #(#destructuring,)* ) = self.fields;
                    #builder_name {
                        fields: ( #(#reconstructing,)* ),
                        phantom: self.phantom,
                    }
                }
            })
        } else {
            None
        };

        let strip_bool_fallback_method = if let Some((method_name, param_list, arg_expr)) = strip_bool_fallback {
            Some(quote! {
                #deprecated
                #doc
                #[allow(clippy::used_underscore_binding, clippy::no_effect_underscore_binding)]
                pub fn #method_name #method_generics (self, #param_list) -> #builder_name <#target_generics>
                #method_where_clause
                {
                    let #field_name = (#arg_expr,);
                    let ( #(#destructuring,)* ) = self.fields;
                    #builder_name {
                        fields: ( #(#reconstructing,)* ),
                        phantom: self.phantom,
                    }
                }
            })
        } else {
            None
        };

        Ok(quote! {
            #[allow(dead_code, non_camel_case_types, missing_docs)]
            #[automatically_derived]
            impl #impl_generics #builder_name <#ty_generics> #where_clause {
                #deprecated
                #doc
                #[allow(clippy::used_underscore_binding, clippy::no_effect_underscore_binding)]
                pub fn #method_name #method_generics (self, #param_list) -> #builder_name <#target_generics>
                #method_where_clause
                {
                    let #field_name = (#arg_expr,);
                    let ( #(#destructuring,)* ) = self.fields;
                    #builder_name {
                        fields: ( #(#reconstructing,)* ),
                        phantom: self.phantom,
                    }
                }
                #strip_option_fallback_method
                #strip_bool_fallback_method
            }
            #[doc(hidden)]
            #[allow(dead_code, non_camel_case_types, non_snake_case)]
            #[allow(clippy::exhaustive_enums)]
            pub enum #repeated_fields_error_type_name {}
            #[doc(hidden)]
            #[allow(dead_code, non_camel_case_types, missing_docs)]
            #[automatically_derived]
            impl #impl_generics #builder_name <#target_generics> #where_clause {
                #[deprecated(
                    note = #repeated_fields_error_message
                )]
                #doc
                pub fn #method_name #method_generics (self, _: #repeated_fields_error_type_name) -> #builder_name <#target_generics>
                #method_where_clause
                {
                    self
                }
            }
        })
    }

    fn required_field_impl(&self, field: &FieldInfo) -> TokenStream {
        let StructInfo { ref builder_name, .. } = self;

        let FieldInfo {
            name: ref field_name, ..
        } = field;
        let mut builder_generics: Vec<syn::GenericArgument> = self
            .generics
            .params
            .iter()
            .map(|generic_param| match generic_param {
                syn::GenericParam::Type(type_param) => {
                    let ident = type_param.ident.to_token_stream();
                    syn::parse2(ident).unwrap()
                }
                syn::GenericParam::Lifetime(lifetime_def) => syn::GenericArgument::Lifetime(lifetime_def.lifetime.clone()),
                syn::GenericParam::Const(const_param) => {
                    let ident = const_param.ident.to_token_stream();
                    syn::parse2(ident).unwrap()
                }
            })
            .collect();
        let mut builder_generics_tuple = empty_type_tuple();
        let generics = {
            let mut generics = self.generics.clone();
            for f in self.included_fields() {
                if f.builder_attr.default.is_some() || f.builder_attr.via_mutators.is_some() {
                    // `f` is not mandatory - it does not have its own fake `build` method, so `field` will need
                    // to warn about missing `field` regardless of whether `f` is set.
                    assert!(
                        f.ordinal != field.ordinal,
                        "`required_field_impl` called for optional field {}",
                        field.name
                    );
                    generics.params.push(f.generic_ty_param());
                    builder_generics_tuple.elems.push_value(f.type_ident());
                } else if f.ordinal < field.ordinal {
                    // Only add a `build` method that warns about missing `field` if `f` is set. If `f` is not set,
                    // `f`'s `build` method will warn, since it appears earlier in the argument list.
                    builder_generics_tuple.elems.push_value(f.tuplized_type_ty_param());
                } else if f.ordinal == field.ordinal {
                    builder_generics_tuple.elems.push_value(empty_type());
                } else {
                    // `f` appears later in the argument list after `field`, so if they are both missing we will
                    // show a warning for `field` and not for `f` - which means this warning should appear whether
                    // or not `f` is set.
                    generics.params.push(f.generic_ty_param());
                    builder_generics_tuple.elems.push_value(f.type_ident());
                }

                builder_generics_tuple.elems.push_punct(Default::default());
            }
            generics
        };

        builder_generics.push(syn::GenericArgument::Type(builder_generics_tuple.into()));
        let (impl_generics, _, where_clause) = generics.split_for_impl();

        let early_build_error_type_name = syn::Ident::new(
            &format!(
                "{}_Error_Missing_required_field_{}",
                builder_name,
                strip_raw_ident_prefix(field_name.to_string())
            ),
            proc_macro2::Span::call_site(),
        );
        let early_build_error_message = format!("Missing required field {}", field_name);

        let build_method_name = self.build_method_name();
        let build_method_visibility = self.build_method_visibility();

        quote! {
            #[doc(hidden)]
            #[allow(dead_code, non_camel_case_types, non_snake_case)]
            #[allow(clippy::exhaustive_enums)]
            pub enum #early_build_error_type_name {}
            #[doc(hidden)]
            #[allow(dead_code, non_camel_case_types, missing_docs, clippy::panic)]
            #[automatically_derived]
            impl #impl_generics #builder_name < #( #builder_generics ),* > #where_clause {
                #[deprecated(
                    note = #early_build_error_message
                )]
                #build_method_visibility fn #build_method_name(self, _: #early_build_error_type_name) -> ! {
                    panic!()
                }
            }
        }
    }

    fn mutator_impl(
        &self,
        mutator @ Mutator {
            fun: mutator_fn,
            required_fields,
        }: &Mutator,
    ) -> syn::Result<TokenStream> {
        let StructInfo { ref builder_name, .. } = *self;

        let mut required_fields = required_fields.clone();

        let mut ty_generics = self.generic_arguments();
        let mut destructuring = TokenStream::new();
        let mut ty_generics_tuple = empty_type_tuple();
        let mut generics = self.generics.clone();
        let mut mutator_ty_fields = Punctuated::<_, Token![,]>::new();
        let mut mutator_destructure_fields = Punctuated::<_, Token![,]>::new();
        for f @ FieldInfo { name, ty, .. } in self.included_fields() {
            if f.builder_attr.via_mutators.is_some() || required_fields.remove(f.name) {
                ty_generics_tuple.elems.push(f.tuplized_type_ty_param());
                mutator_ty_fields.push(quote!(#name: #ty));
                mutator_destructure_fields.push(name);
                quote!((#name,),).to_tokens(&mut destructuring);
            } else {
                generics.params.push(f.generic_ty_param());
                let generic_argument: syn::Type = f.type_ident();
                ty_generics_tuple.elems.push(generic_argument.clone());
                quote!(#name,).to_tokens(&mut destructuring);
            }
        }
        ty_generics.push(syn::GenericArgument::Type(ty_generics_tuple.into()));
        let (impl_generics, _, where_clause) = generics.split_for_impl();

        let mutator_struct_name = format_ident!("TypedBuilderFieldMutator");

        let ItemFn { attrs, vis, .. } = mutator_fn;
        let sig = mutator.outer_sig(parse_quote!(#builder_name <#ty_generics>));
        let fn_name = &sig.ident;
        let (_fn_impl_generics, fn_ty_generics, _fn_where_clause) = &sig.generics.split_for_impl();
        let fn_call_turbofish = fn_ty_generics.as_turbofish();
        let mutator_args = mutator.arguments();

        // Generics for the mutator - should be similar to the struct's generics
        let m_generics = &self.generics;
        let (m_impl_generics, m_ty_generics, m_where_clause) = m_generics.split_for_impl();
        let m_phantom = phantom_data_for_generics(self.generics);

        Ok(quote! {
            #[allow(dead_code, non_camel_case_types, missing_docs)]
            #[automatically_derived]
            impl #impl_generics #builder_name <#ty_generics> #where_clause {
                #(#attrs)*
                #[allow(clippy::used_underscore_binding, clippy::no_effect_underscore_binding)]
                #vis #sig {
                    struct #mutator_struct_name #m_generics #m_where_clause {
                        __phantom: #m_phantom,
                        #mutator_ty_fields
                    }
                    impl #m_impl_generics #mutator_struct_name #m_ty_generics #m_where_clause {
                        #mutator_fn
                    }

                    let __args = (#mutator_args);

                    let ( #destructuring ) = self.fields;
                    let mut __mutator: #mutator_struct_name #m_ty_generics = #mutator_struct_name {
                        __phantom: ::core::default::Default::default(),
                        #mutator_destructure_fields
                    };

                    // This dance is required to keep mutator args and destrucutre fields from interfering.
                    {
                        let (#mutator_args) = __args;
                        __mutator.#fn_name #fn_call_turbofish(#mutator_args);
                    }

                    let #mutator_struct_name {
                        __phantom,
                        #mutator_destructure_fields
                    } = __mutator;

                    #builder_name {
                        fields: ( #destructuring ),
                        phantom: self.phantom,
                    }
                }
            }
        })
    }

    fn build_method_name(&self) -> TokenStream {
        self.builder_attr.build_method.common.get_name().unwrap_or(quote!(build))
    }

    fn build_method_visibility(&self) -> TokenStream {
        first_visibility(&[self.builder_attr.build_method.common.vis.as_ref(), Some(&public_visibility())])
    }

    fn build_method_impl(&self) -> TokenStream {
        let StructInfo {
            ref name,
            ref builder_name,
            ..
        } = *self;

        let generics = {
            let mut generics = self.generics.clone();
            for field in self.included_fields() {
                if field.builder_attr.default.is_some() {
                    let trait_ref = syn::TraitBound {
                        paren_token: None,
                        lifetimes: None,
                        modifier: syn::TraitBoundModifier::None,
                        path: {
                            let mut path = self.builder_attr.crate_module_path.clone();
                            path.segments.push(syn::PathSegment {
                                ident: Ident::new("Optional", Span::call_site()),
                                arguments: syn::PathArguments::AngleBracketed(syn::AngleBracketedGenericArguments {
                                    colon2_token: None,
                                    lt_token: Default::default(),
                                    args: [syn::GenericArgument::Type(field.ty.clone())].into_iter().collect(),
                                    gt_token: Default::default(),
                                }),
                            });
                            path
                        },
                    };
                    let mut generic_param: syn::TypeParam = field.generic_ident.clone().into();
                    generic_param.bounds.push(trait_ref.into());
                    generics.params.push(generic_param.into());
                }
            }
            generics
        };
        let (impl_generics, _, _) = generics.split_for_impl();

        let (_, ty_generics, where_clause) = self.generics.split_for_impl();

        let modified_ty_generics = modify_types_generics_hack(&ty_generics, |args| {
            args.push(syn::GenericArgument::Type(
                type_tuple(self.included_fields().map(|field| {
                    if field.builder_attr.default.is_some() {
                        field.type_ident()
                    } else {
                        field.tuplized_type_ty_param()
                    }
                }))
                .into(),
            ));
        });

        let destructuring = self.included_fields().map(|f| f.name);

        // The default of a field can refer to earlier-defined fields, which we handle by
        // writing out a bunch of `let` statements first, which can each refer to earlier ones.
        // This means that field ordering may actually be significant, which isn't ideal. We could
        // relax that restriction by calculating a DAG of field default dependencies and
        // reordering based on that, but for now this much simpler thing is a reasonable approach.
        let assignments = self.fields.iter().map(|field| {
            let name = &field.name;

            let maybe_mut = if let Some(span) = field.builder_attr.mutable_during_default_resolution {
                quote_spanned!(span => mut)
            } else {
                quote!()
            };

            if let Some(ref default) = field.builder_attr.default {
                if field.builder_attr.setter.skip.is_some() {
                    quote!(let #maybe_mut #name = #default;)
                } else {
                    let crate_module_path = &self.builder_attr.crate_module_path;

                    quote!(let #maybe_mut #name = #crate_module_path::Optional::into_value(#name, || #default);)
                }
            } else {
                quote!(let #maybe_mut #name = #name.0;)
            }
        });
        let field_names = self.fields.iter().map(|field| field.name);

        let build_method_name = self.build_method_name();
        let build_method_visibility = self.build_method_visibility();
        let build_method_doc = if self.builder_attr.doc {
            self.builder_attr
                .build_method
                .common
                .get_doc_or(|| format!("Finalise the builder and create its [`{}`] instance", name))
        } else {
            quote!()
        };

        let type_constructor = {
            let ty_generics = ty_generics.as_turbofish();
            quote!(#name #ty_generics)
        };

        let (build_method_generic, output_type, build_method_where_clause) = match &self.builder_attr.build_method.into {
            IntoSetting::NoConversion => (None, quote!(#name #ty_generics), None),
            IntoSetting::GenericConversion => (
                Some(quote!(<__R>)),
                quote!(__R),
                Some(quote!(where #name #ty_generics: Into<__R>)),
            ),
            IntoSetting::TypeConversionToSpecificType(into) => (None, into.to_token_stream(), None),
        };

        quote!(
            #[allow(dead_code, non_camel_case_types, missing_docs)]
            #[automatically_derived]
            impl #impl_generics #builder_name #modified_ty_generics #where_clause {
                #build_method_doc
                #[allow(clippy::default_trait_access, clippy::used_underscore_binding, clippy::no_effect_underscore_binding)]
                #build_method_visibility fn #build_method_name #build_method_generic (self) -> #output_type #build_method_where_clause {
                    let ( #(#destructuring,)* ) = self.fields;
                    #( #assignments )*

                    #[allow(deprecated)]
                    #type_constructor {
                        #( #field_names ),*
                    }.into()
                }
            }
        )
    }

    pub fn derive(&self) -> syn::Result<TokenStream> {
        let builder_creation = self.builder_creation_impl()?;
        let fields = self
            .setter_fields()
            .map(|f| self.field_impl(f))
            .collect::<Result<TokenStream, _>>()?;
        let required_fields = self
            .setter_fields()
            .filter(|f| f.builder_attr.default.is_none())
            .map(|f| self.required_field_impl(f));
        let mutators = self
            .fields
            .iter()
            .flat_map(|f| &f.builder_attr.mutators)
            .chain(&self.builder_attr.mutators)
            .map(|m| self.mutator_impl(m))
            .collect::<Result<TokenStream, _>>()?;
        let build_method = self.build_method_impl();

        Ok(quote! {
            #builder_creation
            #fields
            #(#required_fields)*
            #mutators
            #build_method
        })
    }
}
