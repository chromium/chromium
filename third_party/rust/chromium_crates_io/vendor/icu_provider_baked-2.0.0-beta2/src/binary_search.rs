// This file is part of ICU4X. For terms of use, please see the file
// called LICENSE at the top level of the ICU4X source tree
// (online at: https://github.com/unicode-org/icu4x/blob/main/LICENSE ).

//! Data stored as slices, looked up with binary search
//!
//! TODO(#6164): This code is stale; update it before use.

use icu_provider::prelude::*;

#[cfg(feature = "export")]
#[allow(dead_code)]
pub(crate) fn bake(
    marker_bake: &databake::TokenStream,
    bakes_to_ids: Vec<(
        databake::TokenStream,
        std::collections::BTreeSet<DataIdentifierCow>,
    )>,
) -> (databake::TokenStream, usize) {
    use databake::*;
    use proc_macro2::{Ident, Span};

    let mut idents_to_bakes = Vec::new();

    let ids_to_idents = bakes_to_ids
        .iter()
        .flat_map(|(bake, ids)| {
            let min_id = ids.first().unwrap();

            let ident = Ident::new(
                &format!("_{}_{}", min_id.marker_attributes.as_str(), min_id.locale)
                    .chars()
                    .map(|ch| {
                        if ch == '-' {
                            '_'
                        } else {
                            ch.to_ascii_uppercase()
                        }
                    })
                    .collect::<String>(),
                Span::call_site(),
            );

            idents_to_bakes.push((ident.clone(), bake));
            ids.iter().map(move |id| (id.clone(), ident.clone()))
        })
        .collect::<Vec<_>>();

    let mut size = 0;

    // Data.0 is a fat pointer
    size += core::mem::size_of::<&[()]>();

    // The idents are references
    size += ids_to_idents.len() * core::mem::size_of::<&()>();

    let (ty, id_bakes_to_idents) = if ids_to_idents
        .iter()
        .all(|(id, _)| id.marker_attributes.is_empty())
    {
        // Only DataLocales
        size += ids_to_idents.len() * core::mem::size_of::<&str>();
        (
            quote! { icu_provider_baked::binary_search::Locale },
            ids_to_idents
                .iter()
                .map(|(id, ident)| {
                    let k = id.locale.to_string();
                    quote!((#k, #ident))
                })
                .collect::<Vec<_>>(),
        )
    } else if ids_to_idents.iter().all(|(id, _)| id.locale.is_default()) {
        // Only marker attributes
        size += ids_to_idents.len() * core::mem::size_of::<&str>();
        (
            quote! { icu_provider_baked::binary_search::Attributes },
            ids_to_idents
                .iter()
                .map(|(id, ident)| {
                    let k = id.marker_attributes.as_str();
                    quote!((#k, #ident))
                })
                .collect(),
        )
    } else {
        size += ids_to_idents.len() * 2 * core::mem::size_of::<&str>();
        (
            quote! { icu_provider_baked::binary_search::AttributesAndLocale },
            ids_to_idents
                .iter()
                .map(|(id, ident)| {
                    let k0 = id.marker_attributes.as_str();
                    let k1 = id.locale.to_string();
                    quote!(((#k0, #k1), #ident))
                })
                .collect(),
        )
    };

    let idents_to_bakes = idents_to_bakes.into_iter().map(|(ident, bake)| {
        quote! {
            const #ident: &S = &#bake;
        }
    });

    (
        quote! {
            icu_provider_baked::binary_search::Data<#ty, #marker_bake> = {
                type S = <#marker_bake as icu_provider::DynamicDataMarker>::DataStruct;
                #(#idents_to_bakes)*
                icu_provider_baked::binary_search::Data(&[#(#id_bakes_to_idents,)*])
            }
        },
        size,
    )
}

pub struct Data<K: BinarySearchKey, M: DataMarker>(
    pub &'static [(K::Type, &'static M::DataStruct)],
);

impl<K: BinarySearchKey, M: DataMarker> super::DataStore<M> for Data<K, M> {
    fn get(
        &self,
        id: DataIdentifierBorrowed,
        attributes_prefix_match: bool,
    ) -> Option<DataPayload<M>> {
        self.0
            .binary_search_by(|&(k, _)| K::cmp(k, id))
            .or_else(|e| {
                if attributes_prefix_match && e <= self.0.len() {
                    Ok(e)
                } else {
                    Err(e)
                }
            })
            // Safety: binary_search returns in-bounds indices when returning Ok.
            // The err case in `or_else` above only returns in-bounds Ok values
            .map(|i| unsafe { self.0.get_unchecked(i) }.1)
            .map(DataPayload::from_static_ref)
            .ok()
    }

    #[cfg(feature = "alloc")]
    type IterReturn = core::iter::Map<
        core::slice::Iter<'static, (K::Type, &'static M::DataStruct)>,
        fn(&'static (K::Type, &'static M::DataStruct)) -> DataIdentifierCow<'static>,
    >;
    #[cfg(feature = "alloc")]
    fn iter(&self) -> Self::IterReturn {
        self.0.iter().map(|&(k, _)| K::to_id(k))
    }
}

pub trait BinarySearchKey: 'static {
    type Type: Ord + Copy + 'static;

    fn cmp(k: Self::Type, id: DataIdentifierBorrowed) -> core::cmp::Ordering;
    #[cfg(feature = "alloc")]
    fn to_id(k: Self::Type) -> DataIdentifierCow<'static>;
}

pub struct Locale;

impl BinarySearchKey for Locale {
    type Type = &'static str;

    fn cmp(locale: Self::Type, id: DataIdentifierBorrowed) -> core::cmp::Ordering {
        id.locale.strict_cmp(locale.as_bytes()).reverse()
    }

    #[cfg(feature = "alloc")]
    fn to_id(locale: Self::Type) -> DataIdentifierCow<'static> {
        DataIdentifierCow::from_locale(locale.parse().unwrap())
    }
}

pub struct Attributes;

impl BinarySearchKey for Attributes {
    type Type = &'static str;

    fn cmp(attributes: Self::Type, id: DataIdentifierBorrowed) -> core::cmp::Ordering {
        attributes.cmp(id.marker_attributes)
    }

    #[cfg(feature = "alloc")]
    fn to_id(attributes: Self::Type) -> DataIdentifierCow<'static> {
        DataIdentifierCow::from_marker_attributes(DataMarkerAttributes::from_str_or_panic(
            attributes,
        ))
    }
}

pub struct AttributesAndLocale;

impl BinarySearchKey for AttributesAndLocale {
    type Type = (&'static str, &'static str);

    fn cmp((attributes, locale): Self::Type, id: DataIdentifierBorrowed) -> core::cmp::Ordering {
        attributes
            .cmp(id.marker_attributes)
            .then_with(|| id.locale.strict_cmp(locale.as_bytes()).reverse())
    }

    #[cfg(feature = "alloc")]
    fn to_id((attributes, locale): Self::Type) -> DataIdentifierCow<'static> {
        DataIdentifierCow::from_borrowed_and_owned(
            DataMarkerAttributes::from_str_or_panic(attributes),
            locale.parse().unwrap(),
        )
    }
}
