//! Helpers for code generation that don't need macro expansion.

use crate::ir::context::BindgenContext;
use crate::ir::layout::Layout;
use proc_macro2::{Ident, Span, TokenStream};
use quote::TokenStreamExt;

pub mod attributes {
    use proc_macro2::{Ident, Span, TokenStream};
    use std::str::FromStr;

    pub fn repr(which: &str) -> TokenStream {
        let which = Ident::new(which, Span::call_site());
        quote! {
            #[repr( #which )]
        }
    }

    pub fn repr_list(which_ones: &[&str]) -> TokenStream {
        let which_ones = which_ones
            .iter()
            .cloned()
            .map(|one| TokenStream::from_str(one).expect("repr to be valid"));
        quote! {
            #[repr( #( #which_ones ),* )]
        }
    }

    pub fn derives(which_ones: &[&str]) -> TokenStream {
        let which_ones = which_ones
            .iter()
            .cloned()
            .map(|one| TokenStream::from_str(one).expect("derive to be valid"));
        quote! {
            #[derive( #( #which_ones ),* )]
        }
    }

    pub fn inline() -> TokenStream {
        quote! {
            #[inline]
        }
    }

    pub fn must_use() -> TokenStream {
        quote! {
            #[must_use]
        }
    }

    pub fn non_exhaustive() -> TokenStream {
        quote! {
            #[non_exhaustive]
        }
    }

    pub fn doc(comment: String) -> TokenStream {
        // NOTE(emilio): By this point comments are already preprocessed and in
        // `///` form. Quote turns them into `#[doc]` comments, but oh well.
        TokenStream::from_str(&comment).unwrap()
    }

    pub fn link_name(name: &str) -> TokenStream {
        // LLVM mangles the name by default but it's already mangled.
        // Prefixing the name with \u{1} should tell LLVM to not mangle it.
        let name = format!("\u{1}{}", name);
        quote! {
            #[link_name = #name]
        }
    }
}

/// Generates a proper type for a field or type with a given `Layout`, that is,
/// a type with the correct size and alignment restrictions.
pub fn blob(ctx: &BindgenContext, layout: Layout) -> TokenStream {
    let opaque = layout.opaque();

    // FIXME(emilio, #412): We fall back to byte alignment, but there are
    // some things that legitimately are more than 8-byte aligned.
    //
    // Eventually we should be able to `unwrap` here, but...
    let ty_name = match opaque.known_rust_type_for_array(ctx) {
        Some(ty) => ty,
        None => {
            warn!("Found unknown alignment on code generation!");
            "u8"
        }
    };

    let ty_name = Ident::new(ty_name, Span::call_site());

    let data_len = opaque.array_size(ctx).unwrap_or(layout.size);

    if data_len == 1 {
        quote! {
            #ty_name
        }
    } else {
        quote! {
            [ #ty_name ; #data_len ]
        }
    }
}

/// Integer type of the same size as the given `Layout`.
pub fn integer_type(
    ctx: &BindgenContext,
    layout: Layout,
) -> Option<TokenStream> {
    let name = Layout::known_type_for_size(ctx, layout.size)?;
    let name = Ident::new(name, Span::call_site());
    Some(quote! { #name })
}

/// Generates a bitfield allocation unit type for a type with the given `Layout`.
pub fn bitfield_unit(ctx: &BindgenContext, layout: Layout) -> TokenStream {
    let mut tokens = quote! {};

    if ctx.options().enable_cxx_namespaces {
        tokens.append_all(quote! { root:: });
    }

    let size = layout.size;
    tokens.append_all(quote! {
        __BindgenBitfieldUnit<[u8; #size]>
    });

    tokens
}

pub mod ast_ty {
    use crate::ir::context::BindgenContext;
    use crate::ir::function::FunctionSig;
    use crate::ir::layout::Layout;
    use crate::ir::ty::FloatKind;
    use proc_macro2::{self, TokenStream};
    use std::str::FromStr;

    pub fn c_void(ctx: &BindgenContext) -> TokenStream {
        // ctypes_prefix takes precedence
        match ctx.options().ctypes_prefix {
            Some(ref prefix) => {
                let prefix = TokenStream::from_str(prefix.as_str()).unwrap();
                quote! {
                    #prefix::c_void
                }
            }
            None => {
                if ctx.options().use_core &&
                    ctx.options().rust_features.core_ffi_c_void
                {
                    quote! { ::core::ffi::c_void }
                } else {
                    quote! { ::std::os::raw::c_void }
                }
            }
        }
    }

    pub fn raw_type(ctx: &BindgenContext, name: &str) -> TokenStream {
        let ident = ctx.rust_ident_raw(name);
        match ctx.options().ctypes_prefix {
            Some(ref prefix) => {
                let prefix = TokenStream::from_str(prefix.as_str()).unwrap();
                quote! {
                    #prefix::#ident
                }
            }
            None => quote! {
                ::std::os::raw::#ident
            },
        }
    }

    pub fn float_kind_rust_type(
        ctx: &BindgenContext,
        fk: FloatKind,
        layout: Option<Layout>,
    ) -> TokenStream {
        // TODO: we probably should take the type layout into account more
        // often?
        //
        // Also, maybe this one shouldn't be the default?
        match (fk, ctx.options().convert_floats) {
            (FloatKind::Float, true) => quote! { f32 },
            (FloatKind::Double, true) => quote! { f64 },
            (FloatKind::Float, false) => raw_type(ctx, "c_float"),
            (FloatKind::Double, false) => raw_type(ctx, "c_double"),
            (FloatKind::LongDouble, _) => {
                match layout {
                    Some(layout) => {
                        match layout.size {
                            4 => quote! { f32 },
                            8 => quote! { f64 },
                            // TODO(emilio): If rust ever gains f128 we should
                            // use it here and below.
                            _ => super::integer_type(ctx, layout)
                                .unwrap_or(quote! { f64 }),
                        }
                    }
                    None => {
                        debug_assert!(
                            false,
                            "How didn't we know the layout for a primitive type?"
                        );
                        quote! { f64 }
                    }
                }
            }
            (FloatKind::Float128, _) => {
                if ctx.options().rust_features.i128_and_u128 {
                    quote! { u128 }
                } else {
                    quote! { [u64; 2] }
                }
            }
        }
    }

    pub fn int_expr(val: i64) -> TokenStream {
        // Don't use quote! { #val } because that adds the type suffix.
        let val = proc_macro2::Literal::i64_unsuffixed(val);
        quote!(#val)
    }

    pub fn uint_expr(val: u64) -> TokenStream {
        // Don't use quote! { #val } because that adds the type suffix.
        let val = proc_macro2::Literal::u64_unsuffixed(val);
        quote!(#val)
    }

    pub fn byte_array_expr(bytes: &[u8]) -> TokenStream {
        let mut bytes: Vec<_> = bytes.to_vec();
        bytes.push(0);
        quote! { [ #(#bytes),* ] }
    }

    pub fn cstr_expr(mut string: String) -> TokenStream {
        string.push('\0');
        let b = proc_macro2::Literal::byte_string(string.as_bytes());
        quote! {
            #b
        }
    }

    pub fn float_expr(ctx: &BindgenContext, f: f64) -> Result<TokenStream, ()> {
        if f.is_finite() {
            let val = proc_macro2::Literal::f64_unsuffixed(f);

            return Ok(quote!(#val));
        }

        let prefix = ctx.trait_prefix();

        if f.is_nan() {
            return Ok(quote! {
                ::#prefix::f64::NAN
            });
        }

        if f.is_infinite() {
            return Ok(if f.is_sign_positive() {
                quote! {
                    ::#prefix::f64::INFINITY
                }
            } else {
                quote! {
                    ::#prefix::f64::NEG_INFINITY
                }
            });
        }

        warn!("Unknown non-finite float number: {:?}", f);
        Err(())
    }

    pub fn arguments_from_signature(
        signature: &FunctionSig,
        ctx: &BindgenContext,
    ) -> Vec<TokenStream> {
        let mut unnamed_arguments = 0;
        signature
            .argument_types()
            .iter()
            .map(|&(ref name, _ty)| match *name {
                Some(ref name) => {
                    let name = ctx.rust_ident(name);
                    quote! { #name }
                }
                None => {
                    unnamed_arguments += 1;
                    let name =
                        ctx.rust_ident(format!("arg{}", unnamed_arguments));
                    quote! { #name }
                }
            })
            .collect()
    }
}
