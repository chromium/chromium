#![allow(clippy::needless_doctest_main)]

//! [![travis](https://api.travis-ci.org/mexus/unzip-n.svg?branch=master)](https://travis-ci.org/mexus/unzip-n)
//! [![crates.io](https://img.shields.io/crates/v/unzip-n.svg)](https://crates.io/crates/unzip-n)
//! [![docs.rs](https://docs.rs/unzip-n/badge.svg)](https://docs.rs/unzip-n)
//!
//! Procedural macro for unzipping iterators-over-`n`-length-tuples into `n` collections.
//!
//! Here's a brief example of what it is capable of:
//!
//! ```
//! use unzip_n::unzip_n;
//!
//! unzip_n!(pub 3);
//! // // Or simply leave the visibility modifier absent for inherited visibility
//! // // (which usually means "private").
//! // unzip_n!(3);
//!
//! fn main() {
//!     let v = vec![(1, 2, 3), (4, 5, 6)];
//!     let (v1, v2, v3) = v.into_iter().unzip_n_vec();
//!
//!     assert_eq!(v1, &[1, 4]);
//!     assert_eq!(v2, &[2, 5]);
//!     assert_eq!(v3, &[3, 6]);
//! }
//! ```
//!
//! # License
//!
//! Licensed under either of
//!
//! * Apache License, Version 2.0 (LICENSE-APACHE or http://www.apache.org/licenses/LICENSE-2.0)
//! * MIT license (LICENSE-MIT or http://opensource.org/licenses/MIT)
//!
//! at your option.
//!
//! # Contribution
//!
//! Unless you explicitly state otherwise, any contribution intentionally submitted
//! for inclusion in the work by you, as defined in the Apache-2.0 license, shall be dual licensed as above, without any
//! additional terms or conditions.

extern crate proc_macro;

use proc_macro2::TokenStream;
use quote::{format_ident, quote};
use syn::{
    parse::{self, Parse, ParseStream},
    parse_macro_input,
};

struct UnzipN {
    n: usize,
    generic_types: Vec<syn::Ident>,
    collections: Vec<syn::Ident>,
    trait_name: syn::Ident,
    visibility: syn::Visibility,
}

impl Parse for UnzipN {
    fn parse(input: ParseStream) -> parse::Result<Self> {
        let visibility = input.parse()?;
        let n: usize = input.parse::<syn::LitInt>()?.base10_parse()?;
        let generic_types: Vec<_> = (0..n).map(|id| format_ident!("Type_{}", id)).collect();
        let collections: Vec<_> = (0..n)
            .map(|id| format_ident!("Collection_{}", id))
            .collect();
        let trait_name = format_ident!("Unzip{}", n);
        Ok(UnzipN {
            n,
            generic_types,
            collections,
            trait_name,
            visibility,
        })
    }
}

impl UnzipN {
    fn make_trait(&self, no_std: bool) -> TokenStream {
        let trait_doc = format!(
            "Extension trait for unzipping iterators over tuples of size {}.",
            self.n
        );
        let generic_types = &self.generic_types;
        let collections = &self.collections;
        let trait_name = &self.trait_name;
        let visibility = &self.visibility;

        let unzip_n_vec = if no_std {
            TokenStream::new()
        } else {
            quote!(
                /// Unzips an iterator over tuples into a tuple of vectors.
                fn unzip_n_vec(self) -> ( #( Vec< #generic_types >, )* )
                where
                    Self: Sized,
                {
                    self.unzip_n()
                }
            )
        };

        quote!(
            #[doc = #trait_doc]
            #visibility trait #trait_name < #( #generic_types, )* > {
                /// Unzips an iterator over tuples into a tuple of collections.
                fn unzip_n<#(#collections,)*>(self) -> ( #(#collections,)* )
                    where
                        #( #collections: Default + Extend< #generic_types >, )*
                        ;

                #unzip_n_vec
            }
        )
    }

    fn make_implementation(&self) -> TokenStream {
        let generic_types = &self.generic_types;
        let collections = &self.collections;
        let trait_name = &self.trait_name;

        let containers: Vec<_> = (0..self.n)
            .map(|id| format_ident!("container_{}", id))
            .collect();
        let values: Vec<_> = (0..self.n).map(|id| format_ident!("val_{}", id)).collect();

        quote!(
            impl<Iter, #(#generic_types,)*> #trait_name <#(#generic_types,)*> for Iter
            where
                Iter: Iterator<Item = (#(#generic_types,)*)>,
            {
                fn unzip_n<#(#collections,)*>(self) -> ( #(#collections,)* )
                    where
                        #( #collections: Default + Extend< #generic_types >, )*
                {
                    #( let mut #containers = #collections :: default() ;)*
                    self.for_each(|( #( #values, )* )| {
                        #( #containers.extend(Some(#values)) ;)*
                    });
                    (#( #containers, )* )
                }
            }
        )
    }

    pub fn generate(&self, no_std: bool) -> TokenStream {
        let trait_decl = self.make_trait(no_std);
        let impl_block = self.make_implementation();
        quote!( #trait_decl #impl_block )
    }
}

/// Generates a trait and its implementation to "unzip" an iterator of `N`-sized tuple into `N`
/// collections, where `N` is passed to the macro, like `unzip_n(10)`:
///
/// ```
/// # use std::collections::HashSet;
/// # use unzip_n::unzip_n;
/// // Note that visiblity modifier is accepted!
/// unzip_n!(pub(crate) 2);
/// unzip_n!(5);
/// unzip_n!(3);
///
/// # fn main() {
/// let v = vec![(1, 2), (3, 4)];
/// let (s1, s2): (HashSet<_>, HashSet<_>) = v.clone().into_iter().unzip_n();
/// println!("{:?}, {:?}", s1, s2);
///
/// let (v1, v2) = v.into_iter().unzip_n_vec();
/// println!("{:?}, {:?}", v1, v2);
///
/// let v = vec![(1, 2, 3, 4, 5), (6, 7, 8, 9, 10)];
/// let (v1, v2, v3, v4, v5) = v.into_iter().unzip_n_vec();
/// println!("{:?}, {:?}, {:?}, {:?}, {:?}", v1, v2, v3, v4, v5);
/// # }
/// ```
///
/// For example, `unzip_n(3)` will produce a code like the following:
///
/// ```
/// ///Extension trait for unzipping iterators over tuples of size 3.
/// trait Unzip3<Type_0, Type_1, Type_2> {
///     /// Unzips an iterator over tuples into a tuple of collections.
///     fn unzip_n<Collection_0, Collection_1, Collection_2>(
///         self,
///     ) -> (Collection_0, Collection_1, Collection_2)
///     where
///         Collection_0: Default + Extend<Type_0>,
///         Collection_1: Default + Extend<Type_1>,
///         Collection_2: Default + Extend<Type_2>;
///     /// Unzips an iterator over tuples into a tuple of vectors.
///     fn unzip_n_vec(self) -> (Vec<Type_0>, Vec<Type_1>, Vec<Type_2>)
///     where
///         Self: Sized,
///     {
///         self.unzip_n()
///     }
/// }
///
/// impl<Iter, Type_0, Type_1, Type_2> Unzip3<Type_0, Type_1, Type_2> for Iter
/// where
///     Iter: Iterator<Item = (Type_0, Type_1, Type_2)>,
/// {
///     fn unzip_n<Collection_0, Collection_1, Collection_2>(
///         self,
///     ) -> (Collection_0, Collection_1, Collection_2)
///     where
///         Collection_0: Default + Extend<Type_0>,
///         Collection_1: Default + Extend<Type_1>,
///         Collection_2: Default + Extend<Type_2>,
///     {
///         let mut container_0 = Collection_0::default();
///         let mut container_1 = Collection_1::default();
///         let mut container_2 = Collection_2::default();
///         self.for_each(|(val_0, val_1, val_2)| {
///             container_0.extend(Some(val_0));
///             container_1.extend(Some(val_1));
///             container_2.extend(Some(val_2));
///         });
///         (container_0, container_1, container_2)
///     }
/// }
/// ```
#[proc_macro]
pub fn unzip_n(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    parse_macro_input!(input as UnzipN).generate(false).into()
}

/// A *no-std* version of the `unzip_n` macro, i.e. without the `unzip_n_vec` trait method.
#[proc_macro]
pub fn unzip_n_nostd(input: proc_macro::TokenStream) -> proc_macro::TokenStream {
    parse_macro_input!(input as UnzipN).generate(true).into()
}
