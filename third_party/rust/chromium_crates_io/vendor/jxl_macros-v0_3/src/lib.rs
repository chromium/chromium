// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

extern crate proc_macro;

use proc_macro::TokenStream;
use proc_macro_error2::{abort, proc_macro_error};
use proc_macro2::TokenStream as TokenStream2;
use quote::quote;
use syn::{DeriveInput, Meta, parse_macro_input};

fn get_bits(expr_call: &syn::ExprCall) -> syn::Expr {
    if let syn::Expr::Path(ep) = &*expr_call.func {
        if !ep.path.is_ident("Bits") {
            abort!(
                expr_call,
                "Unexpected function name in coder: {}",
                ep.path.get_ident().unwrap()
            );
        }
        if expr_call.args.len() != 1 {
            abort!(
                expr_call,
                "Unexpected number of arguments for Bits() in coder: {}",
                expr_call.args.len()
            );
        }
        return expr_call.args[0].clone();
    }
    abort!(expr_call, "Unexpected function call in coder");
}

fn parse_single_coder(input: &syn::Expr, extra_lit: Option<&syn::ExprLit>) -> TokenStream2 {
    match &input {
        syn::Expr::Lit(lit) => match extra_lit {
            None => quote! {U32::Val(#lit)},
            Some(elit) => quote! {U32::Val(#lit + #elit)},
        },
        syn::Expr::Call(expr_call) => {
            let bits = get_bits(expr_call);
            match extra_lit {
                None => quote! {U32::Bits(#bits)},
                Some(elit) => quote! {U32::BitsOffset{n: #bits, off: #elit}},
            }
        }
        syn::Expr::Binary(syn::ExprBinary {
            attrs: _,
            left,
            op: syn::BinOp::Add(_),
            right,
        }) => {
            let (left, right) = if let syn::Expr::Lit(_) = **left {
                (right, left)
            } else {
                (left, right)
            };
            match (&**left, &**right) {
                (syn::Expr::Call(expr_call), syn::Expr::Lit(lit)) => {
                    let bits = get_bits(expr_call);
                    match extra_lit {
                        None => quote! {U32::BitsOffset{n: #bits, off: #lit}},
                        Some(elit) => quote! {U32::BitsOffset{n: #bits, off: #lit + #elit}},
                    }
                }
                _ => abort!(
                    input,
                    "Unexpected expression in coder, must be Bits(a) + b, Bits(a), or b"
                ),
            }
        }
        _ => abort!(
            input,
            "Unexpected expression in coder, must be Bits(a) + b, Bits(a), or b"
        ),
    }
}

fn parse_coder(input: &syn::Expr) -> TokenStream2 {
    let parse_u2s = |expr_call: &syn::ExprCall, lit: Option<&syn::ExprLit>| {
        if let syn::Expr::Path(ep) = &*expr_call.func {
            if !ep.path.is_ident("u2S") {
                let coder = parse_single_coder(input, None);
                return quote! {U32Coder::Direct(#coder)};
            }
            if expr_call.args.len() != 4 {
                abort!(
                    input,
                    "Unexpected number of arguments for U32() in coder: {}",
                    expr_call.args.len()
                );
            }
            let args = vec![
                parse_single_coder(&expr_call.args[0], lit),
                parse_single_coder(&expr_call.args[1], lit),
                parse_single_coder(&expr_call.args[2], lit),
                parse_single_coder(&expr_call.args[3], lit),
            ];
            return quote! {U32Coder::Select(#(#args),*)};
        }
        abort!(input, "Unexpected function call in coder");
    };

    match &input {
        syn::Expr::Call(expr_call) => parse_u2s(expr_call, None),
        syn::Expr::Binary(syn::ExprBinary {
            attrs: _,
            left,
            op: syn::BinOp::Add(_),
            right,
        }) => {
            let (left, right) = if let syn::Expr::Lit(_) = **left {
                (right, left)
            } else {
                (left, right)
            };
            match (&**left, &**right) {
                (syn::Expr::Call(expr_call), syn::Expr::Lit(lit)) => {
                    parse_u2s(expr_call, Some(lit))
                }
                _ => abort!(
                    input,
                    "Unexpected expression in coder, must be (u2S|Bits)(a) + b, (u2S|Bits)(a), or b"
                ),
            }
        }
        _ => parse_single_coder(input, None),
    }
}

fn parse_size_coder(mut input: syn::Expr) -> TokenStream2 {
    match input {
        syn::Expr::Call(syn::ExprCall {
            ref func,
            ref mut args,
            ..
        }) => {
            if args.len() != 1 {
                abort!(input, "Expected 1 argument in sized_coder inner call");
            }

            match &**func {
                syn::Expr::Path(expr_path) if expr_path.path.is_ident("implicit") => {
                    let arg = args.first().unwrap().clone();
                    parse_coder(&arg)
                }
                syn::Expr::Path(expr_path) if expr_path.path.is_ident("explicit") => {
                    quote! { U32Coder::Direct(U32::Val(#args)) }
                }
                _ => abort!(
                    input,
                    "Unexpected expression in size_coder, must be 'implicit()' or 'explicit()'"
                ),
            }
        }
        _ => abort!(
            input,
            "Unexpected expression in size_coder, must be 'implicit()' or 'explicit()'"
        ),
    }
}

fn prettify_condition(cond: &syn::Expr) -> String {
    (quote! {#cond})
        .to_string()
        .replace(" . ", ".")
        .replace("! ", "!")
        .replace(" :: ", "::")
}

#[derive(Debug)]
struct Condition {
    expr: Option<syn::Expr>,
    has_all_default: bool,
    pretty: String,
}

impl Condition {
    fn get_expr(&self, all_default_field: &Option<syn::Ident>) -> Option<TokenStream2> {
        if self.has_all_default {
            let all_default = all_default_field.as_ref().unwrap();
            match &self.expr {
                Some(expr) => Some(quote! { !#all_default && (#expr) }),
                None => Some(quote! { !#all_default }),
            }
        } else {
            self.expr.as_ref().map(|expr| quote! {#expr})
        }
    }
    fn get_pretty(&self, all_default_field: &Option<syn::Ident>) -> String {
        if self.has_all_default {
            let all_default = all_default_field.as_ref().unwrap();
            let all_default = "!".to_owned() + &quote! {#all_default}.to_string();
            match &self.expr {
                Some(_) => all_default + " && (" + &self.pretty + ")",
                None => all_default,
            }
        } else {
            self.pretty.clone()
        }
    }
}

#[derive(Debug, Clone)]
struct U32 {
    coder: TokenStream2,
}

#[derive(Debug)]
#[allow(clippy::large_enum_variant)]
enum Coder {
    WithoutConfig,
    U32(U32),
    Select(Condition, U32, U32),
    Vector(U32, Box<Coder>),
}

impl Coder {
    fn ty(&self) -> TokenStream2 {
        match self {
            Coder::WithoutConfig => quote! {()},
            Coder::U32(..) => quote! {U32Coder},
            Coder::Select(..) => quote! {U32Coder},
            Coder::Vector(_, value_coder) => {
                let value_coder_ty = value_coder.ty();
                quote! {VectorCoder<#value_coder_ty>}
            }
        }
    }

    fn config(&self, all_default_field: &Option<syn::Ident>) -> TokenStream2 {
        match self {
            Coder::WithoutConfig => quote! { () },
            Coder::U32(U32 { coder }) => quote! { #coder },
            Coder::Select(condition, U32 { coder: coder_true }, U32 { coder: coder_false }) => {
                let cnd = condition.get_expr(all_default_field).unwrap();
                quote! {
                    if #cnd { #coder_true } else { #coder_false }
                }
            }
            Coder::Vector(U32 { coder }, value_coder) => {
                let value_coder = value_coder.config(all_default_field);
                quote! {VectorCoder{size_coder: #coder, value_coder: #value_coder}}
            }
        }
    }
}

#[derive(Debug)]
enum FieldKind {
    Unconditional(Coder),
    Conditional(Condition, Coder),
    Defaulted(Condition, Coder),
}

#[derive(Debug)]
struct Field {
    name: proc_macro2::Ident,
    kind: FieldKind,
    ty: syn::Type,
    default: Option<TokenStream2>,
    default_element: Option<TokenStream2>,
    nonserialized_inits: Vec<TokenStream2>,
}

impl Field {
    fn parse(f: &syn::Field, num: usize, all_default_field: &mut Option<syn::Ident>) -> Field {
        let mut condition = None;
        let mut default = None;
        let mut coder = None;

        let mut select_coder = None;
        let mut coder_true = None;
        let mut coder_false = None;

        let mut is_all_default = false;

        let mut size_coder = None;

        let mut nonserialized = vec![];

        let mut default_element = None;

        // Parse attributes.
        for a in &f.attrs {
            match a.path().get_ident().map(syn::Ident::to_string).as_deref() {
                Some("coder") => {
                    if coder.is_some() {
                        abort!(f, "Repeated coder");
                    }
                    let coder_ast = a.parse_args::<syn::Expr>().unwrap();
                    coder = Some(Coder::U32(U32 {
                        coder: parse_coder(&coder_ast),
                    }));
                }
                Some("default") => {
                    if default.is_some() {
                        abort!(f, "Repeated default");
                    }
                    let default_expr = a.parse_args::<syn::Expr>().unwrap();
                    default = Some(quote! {#default_expr});
                }
                Some("default_element") => {
                    if default_element.is_some() {
                        abort!(f, "Repeated default_element")
                    }
                    let default_element_expr = a.parse_args::<syn::Expr>().unwrap();
                    default_element = Some(quote! { #default_element_expr })
                }
                Some("condition") => {
                    if condition.is_some() {
                        abort!(f, "Repeated condition");
                    }
                    let condition_ast = a.parse_args::<syn::Expr>().unwrap();
                    let pretty_cond = prettify_condition(&condition_ast);
                    condition = Some(Condition {
                        expr: Some(condition_ast),
                        has_all_default: all_default_field.is_some(),
                        pretty: pretty_cond,
                    });
                }
                Some("all_default") => {
                    if num != 0 {
                        abort!(f, "all_default is not the first field");
                    }
                    if default.is_some() {
                        abort!(f, "all_default has an implicit default");
                    }
                    is_all_default = true;
                    default = Some(quote! { true });
                }
                Some("select_coder") => {
                    if select_coder.is_some() {
                        abort!(f, "Repeated select_coder");
                    }
                    let condition_ast = a.parse_args::<syn::Expr>().unwrap();
                    let pretty_cond = prettify_condition(&condition_ast);
                    select_coder = Some(Condition {
                        expr: Some(condition_ast),
                        has_all_default: false,
                        pretty: pretty_cond,
                    });
                }
                Some("coder_false") => {
                    if coder_false.is_some() {
                        abort!(f, "Repeated coder_false");
                    }
                    let coder_ast = a.parse_args::<syn::Expr>().unwrap();
                    coder_false = Some(U32 {
                        coder: parse_coder(&coder_ast),
                    });
                }
                Some("coder_true") => {
                    if coder_true.is_some() {
                        abort!(f, "Repeated coder_true");
                    }
                    let coder_ast = a.parse_args::<syn::Expr>().unwrap();
                    coder_true = Some(U32 {
                        coder: parse_coder(&coder_ast),
                    });
                }
                Some("size_coder") => {
                    if size_coder.is_some() {
                        abort!(f, "Repeated size_coder");
                    }
                    let coder_ast = a.parse_args::<syn::Expr>().unwrap();
                    size_coder = Some(U32 {
                        coder: parse_size_coder(coder_ast),
                    });
                }
                Some("nonserialized") => {
                    let Meta::List(ns) = &a.meta else {
                        abort!(a, "Invalid attribute");
                    };
                    let stream = &ns.tokens;
                    nonserialized.push(quote! {#stream});
                }
                _ => {}
            }
        }

        if default.is_some() && default_element.is_some() {
            abort!(f, "default is incompatible with default_element");
        }

        if let Some(select_coder) = select_coder {
            if coder_true.is_none() || coder_false.is_none() {
                abort!(
                    f,
                    "Invalid field, select_coder is set but coder_true or coder_false are not"
                )
            }
            if coder.is_some() {
                abort!(f, "Invalid field, select_coder and coder are both present")
            }
            coder = Some(Coder::Select(
                select_coder,
                coder_true.unwrap(),
                coder_false.unwrap(),
            ))
        }

        let condition = if condition.is_some() || all_default_field.is_none() {
            condition
        } else {
            Some(Condition {
                expr: None,
                has_all_default: true,
                pretty: String::new(),
            })
        };

        // Assume nested field if no coder.
        let mut coder = coder.unwrap_or_else(|| Coder::WithoutConfig);

        if let Some(c) = size_coder {
            if default.is_none() {
                default = Some(quote! { Vec::new() });
            }

            coder = Coder::Vector(c, Box::new(coder))
        }

        let ident = f.ident.as_ref().unwrap();

        let kind = match (condition, default.is_some()) {
            (None, _) => FieldKind::Unconditional(coder),
            (Some(cond), false) => FieldKind::Conditional(cond, coder),
            (Some(cond), true) => FieldKind::Defaulted(cond, coder),
        };
        if is_all_default {
            *all_default_field = Some(f.ident.as_ref().unwrap().clone());
        }
        Field {
            name: ident.clone(),
            kind,
            ty: f.ty.clone(),
            default,
            default_element,
            nonserialized_inits: nonserialized,
        }
    }

    // Produces reading code (possibly with tracing).
    fn read_fun(&self, all_default_field: &Option<syn::Ident>) -> TokenStream2 {
        let ident = &self.name;
        let ty = &self.ty;
        let nonserialized_inits = &self.nonserialized_inits;
        match &self.kind {
            FieldKind::Unconditional(coder) => {
                let cfg_ty = coder.ty();
                let cfg = coder.config(all_default_field);
                let trc = quote! {
                    crate::util::tracing_wrappers::trace!("Setting {} to {:?}. total_bits_read: {}, peek: {:08b}", stringify!(#ident), #ident, br.total_bits_read(), br.peek(8));
                };
                quote! {
                    let #ident = {
                        let cfg = #cfg;
                        type NS = <#ty as UnconditionalCoder<#cfg_ty>>::Nonserialized;
                        let nonserialized = NS { #(#nonserialized_inits),* };
                        <#ty>::read_unconditional(&cfg, br, &nonserialized)?
                    };
                    #trc
                }
            }
            FieldKind::Conditional(condition, coder) => {
                let cfg_ty = coder.ty();
                let cfg = coder.config(all_default_field);
                let cnd = condition.get_expr(all_default_field).unwrap();
                let pretty_cnd = condition.get_pretty(all_default_field);
                let trc = quote! {
                    crate::util::tracing_wrappers::trace!("{} is {}, setting {} to {:?}. total_bits_read: {}, peek {:08b}", #pretty_cnd, #cnd, stringify!(#ident), #ident, br.total_bits_read(), br.peek(8));
                };
                quote! {
                    let #ident = {
                        let cond = #cnd;
                        let cfg = #cfg;
                        type NS = <#ty as ConditionalCoder<#cfg_ty>>::Nonserialized;
                        let nonserialized = NS { #(#nonserialized_inits),* };
                        <#ty>::read_conditional(&cfg, cond, br, &nonserialized)?
                    };
                    #trc
                }
            }
            FieldKind::Defaulted(condition, coder) => {
                let cfg_ty = coder.ty();
                let cfg = coder.config(all_default_field);
                let cnd = condition.get_expr(all_default_field).unwrap();
                let pretty_cnd = condition.get_pretty(all_default_field);
                let default = &self.default;
                let trc = quote! {
                    crate::util::tracing_wrappers::trace!("{} is {}, setting {} to {:?}. total_bits_read: {}, peek {:08b}", #pretty_cnd, #cnd, stringify!(#ident), #ident, br.total_bits_read(), br.peek(8));
                };

                let (read_fn, default) = if let Some(def) = &self.default_element {
                    (quote! { read_defaulted_element }, Some(def))
                } else {
                    (quote! { read_defaulted }, default.as_ref())
                };

                quote! {
                    let #ident = {
                        let cond = #cnd;
                        let cfg = #cfg;
                        type NS = <#ty as DefaultedCoder<#cfg_ty>>::Nonserialized;
                        let field_nonserialized = NS { #(#nonserialized_inits),* };
                        let default = #default;
                        <#ty>::#read_fn(&cfg, cond, default, br, &field_nonserialized)?
                    };
                    #trc
                }
            }
        }
    }

    // Produces default code.
    fn default_code(&self) -> TokenStream2 {
        let ident = &self.name;
        let ty = &self.ty;
        let nonserialized_inits = &self.nonserialized_inits;
        let default = &self.default;
        match &self.kind {
            FieldKind::Defaulted(_, coder) => {
                let cfg_ty = coder.ty();
                let default = &self.default;

                quote! {
                    let #ident = {
                        type NS = <#ty as DefaultedCoder<#cfg_ty>>::Nonserialized;
                        let field_nonserialized = NS { #(#nonserialized_inits),* };
                        #default
                    };
                }
            }
            _ => quote! { let #ident = #default; },
        }
    }
}

fn derive_struct(input: &DeriveInput) -> TokenStream2 {
    let name = &input.ident;

    let validate = input.attrs.iter().any(|a| a.path().is_ident("validate"));
    let nonserialized: Vec<_> = input
        .attrs
        .iter()
        .filter_map(|a| {
            if a.path().is_ident("nonserialized") {
                Some(a.parse_args::<syn::Expr>().unwrap())
            } else {
                None
            }
        })
        .collect();
    if nonserialized.len() > 1 {
        abort!(input, "repeated nonserialized");
    }
    let nonserialized = if nonserialized.is_empty() {
        quote! {Empty}
    } else {
        let v = &nonserialized[0];
        quote! {#v}
    };

    let data = if let syn::Data::Struct(struct_data) = &input.data {
        struct_data
    } else {
        abort!(input, "derive_struct didn't get a struct");
    };

    let fields = if let syn::Fields::Named(syn::FieldsNamed {
        brace_token: _,
        named,
    }) = &data.fields
    {
        named
    } else {
        abort!(data.fields, "only named fields are supported (for now?)");
    };

    let mut all_default_field = None;

    let fields: Vec<_> = fields
        .iter()
        .enumerate()
        .map(|(n, f)| Field::parse(f, n, &mut all_default_field))
        .collect();
    let fields_read = fields.iter().map(|x| x.read_fun(&all_default_field));
    let fields_names = fields.iter().map(|x| &x.name);

    let impl_default = if fields.iter().all(|x| x.default.is_some()) {
        let field_init = fields.iter().map(Field::default_code);
        let struct_init = fields.iter().map(|f| {
            let ident = &f.name;
            quote! { #ident }
        });
        quote! {
            impl #name {
                pub fn default(nonserialized: &#nonserialized) -> #name {
                    #(#field_init)*
                    #name {
                        #(#struct_init),*
                    }
                }
            }

        }
    } else {
        quote! {}
    };

    let impl_validate = if validate {
        quote! { return_value.check(nonserialized)?; }
    } else {
        quote! {}
    };

    let align = match input.attrs.iter().any(|a| a.path().is_ident("aligned")) {
        true => quote! { br.jump_to_byte_boundary()?; },
        false => quote! {},
    };

    quote! {
        #impl_default
        impl crate::headers::encodings::UnconditionalCoder<()> for #name {
            type Nonserialized = #nonserialized;
            #[cold]
            #[inline(never)]
            fn read_unconditional(_: &(), br: &mut BitReader, nonserialized: &Self::Nonserialized) -> Result<#name, Error> {
                use crate::headers::encodings::UnconditionalCoder;
                use crate::headers::encodings::ConditionalCoder;
                use crate::headers::encodings::DefaultedCoder;
                use crate::headers::encodings::DefaultedElementCoder;
                #align
                #(#fields_read)*
                let return_value = #name {
                    #(#fields_names),*
                };
                #impl_validate
                Ok(return_value)
            }
        }
    }
}

fn derive_enum(input: &DeriveInput) -> TokenStream2 {
    let name = &input.ident;
    quote! {
        impl crate::headers::encodings::UnconditionalCoder<U32Coder> for #name {
            type Nonserialized = Empty;
            fn read_unconditional(config: &U32Coder, br: &mut BitReader, _: &Empty) -> Result<#name, Error> {
                use num_traits::FromPrimitive;
                let u = u32::read_unconditional(config, br, &Empty{})?;
                if let Some(e) =  #name::from_u32(u) {
                    Ok(e)
                } else {
                    Err(Error::InvalidEnum(u, stringify!(#name).to_string()))
                }
            }
        }
        impl crate::headers::encodings::UnconditionalCoder<()> for #name {
            type Nonserialized = Empty;
            fn read_unconditional(config: &(), br: &mut BitReader, nonserialized: &Empty) -> Result<#name, Error> {
                #name::read_unconditional(
                    &U32Coder::Select(
                        U32::Val(0), U32::Val(1),
                        U32::BitsOffset{n: 4, off: 2},
                        U32::BitsOffset{n: 6, off: 18}), br, nonserialized)
            }
        }
    }
}

#[proc_macro_error]
#[proc_macro_derive(
    UnconditionalCoder,
    attributes(
        coder,
        condition,
        default,
        default_element,
        all_default,
        select_coder,
        coder_true,
        coder_false,
        validate,
        size_coder,
        nonserialized,
        aligned,
    )
)]
pub fn derive_jxl_headers(input: TokenStream) -> TokenStream {
    let input = parse_macro_input!(input as DeriveInput);

    match &input.data {
        syn::Data::Struct(_) => derive_struct(&input).into(),
        syn::Data::Enum(_) => derive_enum(&input).into(),
        _ => abort!(input, "Only implemented for struct"),
    }
}

#[proc_macro_attribute]
pub fn noop(_attr: TokenStream, item: TokenStream) -> TokenStream {
    item
}

#[cfg(feature = "test")]
#[proc_macro]
pub fn for_each_test_file(input: TokenStream) -> TokenStream {
    use std::{fs, path::Path};
    use syn::Ident;

    let fn_name = parse_macro_input!(input as Ident);
    let root_test_dir = Path::new(env!("CARGO_MANIFEST_DIR"))
        .join("..")
        .join("jxl")
        .join("resources")
        .join("test");
    let conformance_test_dir = root_test_dir.join("conformance_test_images");

    let mut tests = vec![];

    for test_dir in [root_test_dir, conformance_test_dir] {
        for entry in fs::read_dir(&test_dir).unwrap() {
            let entry = entry.unwrap();
            let path = entry.path();
            if path.extension().is_some_and(|ext| ext == "jxl") {
                let pathname = path.to_string_lossy();
                let relative_path = path
                    .strip_prefix(&test_dir)
                    .unwrap()
                    .to_string_lossy()
                    .replace('/', "_slash_");
                let test_name = format!(
                    "{}_{}",
                    fn_name,
                    relative_path.strip_suffix(".jxl").unwrap()
                );
                let test_name = Ident::new(&test_name, fn_name.span());
                tests.push(quote! {
                    #[test]
                    fn #test_name() {
                        #fn_name(&Path::new(#pathname)).unwrap()
                    }
                });
            }
        }
    }

    quote! {
        #(#tests)*
    }
    .into()
}
