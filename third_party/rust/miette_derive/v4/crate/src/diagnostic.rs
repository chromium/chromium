use proc_macro2::TokenStream;
use quote::quote;
use syn::{punctuated::Punctuated, DeriveInput, Token};

use crate::code::Code;
use crate::diagnostic_arg::DiagnosticArg;
use crate::forward::{Forward, WhichFn};
use crate::help::Help;
use crate::label::Labels;
use crate::related::Related;
use crate::severity::Severity;
use crate::source_code::SourceCode;
use crate::url::Url;

pub enum Diagnostic {
    Struct {
        generics: syn::Generics,
        ident: syn::Ident,
        fields: syn::Fields,
        args: DiagnosticDefArgs,
    },
    Enum {
        ident: syn::Ident,
        generics: syn::Generics,
        variants: Vec<DiagnosticDef>,
    },
}

pub struct DiagnosticDef {
    pub ident: syn::Ident,
    pub fields: syn::Fields,
    pub args: DiagnosticDefArgs,
}

pub enum DiagnosticDefArgs {
    Transparent(Forward),
    Concrete(Box<DiagnosticConcreteArgs>),
}

impl DiagnosticDefArgs {
    pub(crate) fn forward_or_override_enum(
        &self,
        variant: &syn::Ident,
        which_fn: WhichFn,
        mut f: impl FnMut(&DiagnosticConcreteArgs) -> Option<TokenStream>,
    ) -> Option<TokenStream> {
        match self {
            Self::Transparent(forward) => Some(forward.gen_enum_match_arm(variant, which_fn)),
            Self::Concrete(concrete) => f(concrete).or_else(|| {
                concrete
                    .forward
                    .as_ref()
                    .map(|forward| forward.gen_enum_match_arm(variant, which_fn))
            }),
        }
    }
}

#[derive(Default)]
pub struct DiagnosticConcreteArgs {
    pub code: Option<Code>,
    pub severity: Option<Severity>,
    pub help: Option<Help>,
    pub labels: Option<Labels>,
    pub source_code: Option<SourceCode>,
    pub url: Option<Url>,
    pub forward: Option<Forward>,
    pub related: Option<Related>,
}

impl DiagnosticConcreteArgs {
    fn for_fields(fields: &syn::Fields) -> Result<Self, syn::Error> {
        let labels = Labels::from_fields(fields)?;
        let source_code = SourceCode::from_fields(fields)?;
        let related = Related::from_fields(fields)?;
        Ok(DiagnosticConcreteArgs {
            code: None,
            help: None,
            related,
            severity: None,
            labels,
            url: None,
            forward: None,
            source_code,
        })
    }

    fn add_args(
        &mut self,
        attr: &syn::Attribute,
        args: impl Iterator<Item = DiagnosticArg>,
        errors: &mut Vec<syn::Error>,
    ) {
        for arg in args {
            match arg {
                DiagnosticArg::Transparent => {
                    errors.push(syn::Error::new_spanned(attr, "transparent not allowed"));
                }
                DiagnosticArg::Forward(to_field) => {
                    if self.forward.is_some() {
                        errors.push(syn::Error::new_spanned(
                            attr,
                            "forward has already been specified",
                        ));
                    }
                    self.forward = Some(to_field);
                }
                DiagnosticArg::Code(new_code) => {
                    if self.code.is_some() {
                        errors.push(syn::Error::new_spanned(
                            attr,
                            "code has already been specified",
                        ));
                    }
                    self.code = Some(new_code);
                }
                DiagnosticArg::Severity(sev) => {
                    if self.severity.is_some() {
                        errors.push(syn::Error::new_spanned(
                            attr,
                            "severity has already been specified",
                        ));
                    }
                    self.severity = Some(sev);
                }
                DiagnosticArg::Help(hl) => {
                    if self.help.is_some() {
                        errors.push(syn::Error::new_spanned(
                            attr,
                            "help has already been specified",
                        ));
                    }
                    self.help = Some(hl);
                }
                DiagnosticArg::Url(u) => {
                    if self.url.is_some() {
                        errors.push(syn::Error::new_spanned(
                            attr,
                            "url has already been specified",
                        ));
                    }
                    self.url = Some(u);
                }
            }
        }
    }
}

impl DiagnosticDefArgs {
    fn parse(
        _ident: &syn::Ident,
        fields: &syn::Fields,
        attrs: &[&syn::Attribute],
        allow_transparent: bool,
    ) -> syn::Result<Self> {
        let mut errors = Vec::new();

        // Handle the only condition where Transparent is allowed
        if allow_transparent && attrs.len() == 1 {
            if let Ok(args) =
                attrs[0].parse_args_with(Punctuated::<DiagnosticArg, Token![,]>::parse_terminated)
            {
                if matches!(args.first(), Some(DiagnosticArg::Transparent)) {
                    let forward = Forward::for_transparent_field(fields)?;
                    return Ok(Self::Transparent(forward));
                }
            }
        }

        // Create errors for any appearances of Transparent
        let error_message = if allow_transparent {
            "diagnostic(transparent) not allowed in combination with other args"
        } else {
            "diagnostic(transparent) not allowed here"
        };
        fn is_transparent(d: &DiagnosticArg) -> bool {
            matches!(d, DiagnosticArg::Transparent)
        }

        let mut concrete = DiagnosticConcreteArgs::for_fields(fields)?;
        for attr in attrs {
            let args =
                attr.parse_args_with(Punctuated::<DiagnosticArg, Token![,]>::parse_terminated);
            let args = match args {
                Ok(args) => args,
                Err(error) => {
                    errors.push(error);
                    continue;
                }
            };

            if args.iter().any(is_transparent) {
                errors.push(syn::Error::new_spanned(attr, error_message));
            }

            let args = args
                .into_iter()
                .filter(|x| !matches!(x, DiagnosticArg::Transparent));

            concrete.add_args(attr, args, &mut errors);
        }

        let combined_error = errors.into_iter().reduce(|mut lhs, rhs| {
            lhs.combine(rhs);
            lhs
        });
        if let Some(error) = combined_error {
            Err(error)
        } else {
            Ok(DiagnosticDefArgs::Concrete(Box::new(concrete)))
        }
    }
}

impl Diagnostic {
    pub fn from_derive_input(input: DeriveInput) -> Result<Self, syn::Error> {
        let input_attrs = input
            .attrs
            .iter()
            .filter(|x| x.path.is_ident("diagnostic"))
            .collect::<Vec<&syn::Attribute>>();
        Ok(match input.data {
            syn::Data::Struct(data_struct) => {
                let args = DiagnosticDefArgs::parse(
                    &input.ident,
                    &data_struct.fields,
                    &input_attrs,
                    true,
                )?;

                Diagnostic::Struct {
                    fields: data_struct.fields,
                    ident: input.ident,
                    generics: input.generics,
                    args,
                }
            }
            syn::Data::Enum(syn::DataEnum { variants, .. }) => {
                let mut vars = Vec::new();
                for var in variants {
                    let mut variant_attrs = input_attrs.clone();
                    variant_attrs
                        .extend(var.attrs.iter().filter(|x| x.path.is_ident("diagnostic")));
                    let args =
                        DiagnosticDefArgs::parse(&var.ident, &var.fields, &variant_attrs, true)?;
                    vars.push(DiagnosticDef {
                        ident: var.ident,
                        fields: var.fields,
                        args,
                    });
                }
                Diagnostic::Enum {
                    ident: input.ident,
                    generics: input.generics,
                    variants: vars,
                }
            }
            syn::Data::Union(_) => {
                return Err(syn::Error::new(
                    input.ident.span(),
                    "Can't derive Diagnostic for Unions",
                ))
            }
        })
    }

    pub fn gen(&self) -> TokenStream {
        match self {
            Self::Struct {
                ident,
                fields,
                generics,
                args,
            } => {
                let (impl_generics, ty_generics, where_clause) = &generics.split_for_impl();
                match args {
                    DiagnosticDefArgs::Transparent(forward) => {
                        let code_method = forward.gen_struct_method(WhichFn::Code);
                        let help_method = forward.gen_struct_method(WhichFn::Help);
                        let url_method = forward.gen_struct_method(WhichFn::Url);
                        let labels_method = forward.gen_struct_method(WhichFn::Labels);
                        let source_code_method = forward.gen_struct_method(WhichFn::SourceCode);
                        let severity_method = forward.gen_struct_method(WhichFn::Severity);
                        let related_method = forward.gen_struct_method(WhichFn::Related);

                        quote! {
                            impl #impl_generics miette::Diagnostic for #ident #ty_generics #where_clause {
                                #code_method
                                #help_method
                                #url_method
                                #labels_method
                                #severity_method
                                #source_code_method
                                #related_method
                            }
                        }
                    }
                    DiagnosticDefArgs::Concrete(concrete) => {
                        let forward = |which| {
                            concrete
                                .forward
                                .as_ref()
                                .map(|fwd| fwd.gen_struct_method(which))
                        };
                        let code_body = concrete
                            .code
                            .as_ref()
                            .and_then(|x| x.gen_struct())
                            .or_else(|| forward(WhichFn::Code));
                        let help_body = concrete
                            .help
                            .as_ref()
                            .and_then(|x| x.gen_struct(fields))
                            .or_else(|| forward(WhichFn::Help));
                        let sev_body = concrete
                            .severity
                            .as_ref()
                            .and_then(|x| x.gen_struct())
                            .or_else(|| forward(WhichFn::Severity));
                        let rel_body = concrete
                            .related
                            .as_ref()
                            .and_then(|x| x.gen_struct())
                            .or_else(|| forward(WhichFn::Related));
                        let url_body = concrete
                            .url
                            .as_ref()
                            .and_then(|x| x.gen_struct(ident, fields))
                            .or_else(|| forward(WhichFn::Url));
                        let labels_body = concrete
                            .labels
                            .as_ref()
                            .and_then(|x| x.gen_struct(fields))
                            .or_else(|| forward(WhichFn::Labels));
                        let src_body = concrete
                            .source_code
                            .as_ref()
                            .and_then(|x| x.gen_struct(fields))
                            .or_else(|| forward(WhichFn::SourceCode));
                        quote! {
                            impl #impl_generics miette::Diagnostic for #ident #ty_generics #where_clause {
                                #code_body
                                #help_body
                                #sev_body
                                #rel_body
                                #url_body
                                #labels_body
                                #src_body
                            }
                        }
                    }
                }
            }
            Self::Enum {
                ident,
                generics,
                variants,
            } => {
                let (impl_generics, ty_generics, where_clause) = &generics.split_for_impl();
                let code_body = Code::gen_enum(variants);
                let help_body = Help::gen_enum(variants);
                let sev_body = Severity::gen_enum(variants);
                let labels_body = Labels::gen_enum(variants);
                let src_body = SourceCode::gen_enum(variants);
                let rel_body = Related::gen_enum(variants);
                let url_body = Url::gen_enum(ident, variants);
                quote! {
                    impl #impl_generics miette::Diagnostic for #ident #ty_generics #where_clause {
                        #code_body
                        #help_body
                        #sev_body
                        #labels_body
                        #src_body
                        #rel_body
                        #url_body
                    }
                }
            }
        }
    }
}
