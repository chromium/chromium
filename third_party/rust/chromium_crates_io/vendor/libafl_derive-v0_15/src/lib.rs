//! Derives for `LibAFL`

#![no_std]
#![cfg_attr(not(test), warn(
    missing_debug_implementations,
    missing_docs,
    //trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications,
    //unused_results
))]
#![cfg_attr(test, deny(
    missing_debug_implementations,
    //trivial_casts,
    trivial_numeric_casts,
    unused_extern_crates,
    unused_import_braces,
    unused_qualifications,
    unused_must_use,
    //unused_results
))]
#![cfg_attr(
    test,
    deny(
        bad_style,
        dead_code,
        improper_ctypes,
        non_shorthand_field_patterns,
        no_mangle_generic_items,
        overflowing_literals,
        path_statements,
        patterns_in_fns_without_body,
        unconditional_recursion,
        unused,
        unused_allocation,
        unused_comparisons,
        unused_parens,
        while_true
    )
)]

use proc_macro::TokenStream;
use quote::quote;
use syn::{Data::Struct, DeriveInput, Field, Fields::Named, Type, parse_macro_input};

/// Derive macro to implement `SerdeAny`, to use a type in a `SerdeAnyMap`
#[proc_macro_derive(SerdeAny)]
pub fn libafl_serdeany_derive(input: TokenStream) -> TokenStream {
    let name = parse_macro_input!(input as DeriveInput).ident;
    TokenStream::from(quote! {
        libafl_bolts::impl_serdeany!(#name);
    })
}

/// A derive macro to implement `Display`
///
/// Derive macro to implement [`core::fmt::Display`] for a struct where all fields implement `Display`.
/// The result is the space separated concatenation of all fields' display.
/// Order of declaration is preserved.
/// Specifically handled cases:
/// Options: Some => inner type display None => "".
/// Vec: inner type display space separated concatenation.
/// Generics and other more or less exotic stuff are not supported.
///
/// # Examples
///
/// ```rust
/// use libafl_derive;
///
/// #[derive(libafl_derive::Display)]
/// struct MyStruct {
///     foo: String,
///     bar: Option<u32>,
/// }
/// ```
///
/// The above code will expand to:
///
/// ```rust
/// struct MyStruct {
///     foo: String,
///     bar: Option<u32>,
/// }
///
/// impl core::fmt::Display for MyStruct {
///     fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
///         f.write_fmt(format_args!(" {0}", self.foo))?;
///         if let Some(opt) = &self.bar {
///             f.write_fmt(format_args!(" {0}", opt))?;
///         }
///         Ok(())
///     }
/// }
/// ```
///
/// # Panics
/// Panics for any non-structs.
#[proc_macro_derive(Display)]
pub fn libafl_display(input: TokenStream) -> TokenStream {
    let DeriveInput { ident, data, .. } = parse_macro_input!(input as DeriveInput);

    if let Struct(s) = data {
        if let Named(fields) = s.fields {
            let fields_fmt = fields.named.iter().map(libafl_display_field_by_type);

            return quote! {
                impl core::fmt::Display for #ident {
                    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
                        #(#fields_fmt)*
                        Ok(())
                    }
                }
            }
            .into();
        }
    }
    panic!("Only structs are supported");
}

fn libafl_display_field_by_type(it: &Field) -> proc_macro2::TokenStream {
    let fmt = " {}";
    let ident = &it.ident;
    if let Type::Path(type_path) = &it.ty {
        if type_path.qself.is_none() && type_path.path.segments.len() == 1 {
            let segment = &type_path.path.segments[0];
            if segment.ident == "Option" {
                return quote! {
                    if let Some(opt) = &self.#ident {
                        write!(f, #fmt, opt)?;
                    }
                };
            } else if segment.ident == "Vec" {
                return quote! {
                    for e in &self.#ident {
                        write!(f, #fmt, e)?;
                    }
                };
            }
        }
    }
    quote! {
        write!(f, #fmt, self.#ident)?;
    }
}
