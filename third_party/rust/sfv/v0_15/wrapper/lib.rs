// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use sfv::{BareItemFromInput, KeyRef};
use std::collections::hash_map::{Entry, HashMap};
use std::convert::Infallible;
use std::pin::Pin;

/// C++ bindings
#[cxx::bridge(namespace=sfv)]
mod ffi {
    struct ItemParts<'a> {
        bare_item: Pin<&'a mut BareItem>,
        params: Pin<&'a mut Parameters>,
    }

    struct InnerListParts<'a> {
        items: Pin<&'a mut InnerListItems>,
        params: Pin<&'a mut Parameters>,
    }

    // From the `wrapper_functions` target.
    unsafe extern "C++" {
        include!("third_party/rust/sfv/v0_15/wrapper/functions.h");

        type BareItem;
        type Dictionary;
        type InnerListItems;
        type List;
        type Parameters;

        fn list_append_item(ctx: Pin<&mut List>) -> ItemParts<'_>;
        fn list_append_inner_list(ctx: Pin<&mut List>) -> InnerListParts<'_>;

        fn dictionary_append_item<'a>(ctx: Pin<&'a mut Dictionary>, key: &str) -> ItemParts<'a>;
        fn dictionary_set_item(ctx: Pin<&mut Dictionary>, index: usize) -> ItemParts<'_>;
        fn dictionary_append_inner_list<'a>(
            ctx: Pin<&'a mut Dictionary>,
            key: &str,
        ) -> InnerListParts<'a>;
        fn dictionary_set_inner_list(ctx: Pin<&mut Dictionary>, index: usize)
            -> InnerListParts<'_>;

        fn set_bare_item_boolean(ctx: Pin<&mut BareItem>, val: bool);
        fn set_bare_item_integer(ctx: Pin<&mut BareItem>, val: i64);
        fn set_bare_item_decimal(ctx: Pin<&mut BareItem>, val: f64);
        fn set_bare_item_string(ctx: Pin<&mut BareItem>, val: &str);
        fn set_bare_item_token(ctx: Pin<&mut BareItem>, val: &str);
        fn set_bare_item_byte_sequence(ctx: Pin<&mut BareItem>, val: &[u8]);

        fn inner_list_append_item(ctx: Pin<&mut InnerListItems>) -> ItemParts<'_>;

        fn params_append<'a>(ctx: Pin<&'a mut Parameters>, key: &str) -> Pin<&'a mut BareItem>;
        fn params_get(ctx: Pin<&mut Parameters>, index: usize) -> Pin<&mut BareItem>;
    }

    extern "Rust" {
        fn decode_item(input: &[u8], item: ItemParts<'_>, strict: bool) -> bool;

        fn decode_list(input: &[u8], ctx: Pin<&mut List>, strict: bool) -> bool;

        fn decode_dictionary(input: &[u8], ctx: Pin<&mut Dictionary>, strict: bool) -> bool;
    }
}

fn set_bare_item(out: Pin<&mut ffi::BareItem>, bare_item: BareItemFromInput<'_>) {
    match bare_item {
        BareItemFromInput::Decimal(v) => ffi::set_bare_item_decimal(out, f64::from(v)),
        BareItemFromInput::Integer(v) => ffi::set_bare_item_integer(out, i64::from(v)),
        BareItemFromInput::String(ref v) => ffi::set_bare_item_string(out, v.as_str()),
        BareItemFromInput::ByteSequence(ref v) => ffi::set_bare_item_byte_sequence(out, v),
        BareItemFromInput::Boolean(v) => ffi::set_bare_item_boolean(out, v),
        BareItemFromInput::Token(v) => ffi::set_bare_item_token(out, v.as_str()),
        // RFC 8941 does not define these types; only RFC 9651 does.
        BareItemFromInput::Date(_) | BareItemFromInput::DisplayString(_) => unreachable!(),
    }
}

const SMALL_INDEX_MAP_CAPACITY: usize = 16;

#[allow(clippy::large_enum_variant)]
enum KeyIndexMap<'de> {
    Small { keys: [&'de str; SMALL_INDEX_MAP_CAPACITY], len: usize },
    Large(HashMap<&'de str, usize>),
}

impl<'de> KeyIndexMap<'de> {
    fn new() -> Self {
        Self::Small { keys: [""; SMALL_INDEX_MAP_CAPACITY], len: 0 }
    }

    /// Looks up `key` in the map. Returns `Some(index)` if already present, or
    /// inserts `key` at the next sequential index and returns `None`.
    fn get_or_insert(&mut self, key: &'de str) -> Option<usize> {
        match self {
            Self::Small { keys, len } => {
                if let Some(index) = keys[..*len].iter().position(|&k| k == key) {
                    return Some(index);
                }
                if let Some(slot) = keys.get_mut(*len) {
                    *slot = key;
                    *len += 1;
                    return None;
                }
                let mut map = HashMap::with_capacity(SMALL_INDEX_MAP_CAPACITY * 2);
                map.extend(keys.iter().copied().zip(0..));
                map.insert(key, SMALL_INDEX_MAP_CAPACITY);
                *self = Self::Large(map);
                None
            }
            Self::Large(map) => {
                let next_index = map.len();
                match map.entry(key) {
                    Entry::Occupied(entry) => Some(*entry.get()),
                    Entry::Vacant(entry) => {
                        entry.insert(next_index);
                        None
                    }
                }
            }
        }
    }
}

struct ParamVisitor<'a, 'de> {
    params: Pin<&'a mut ffi::Parameters>,
    indices: KeyIndexMap<'de>,
}

impl<'a> ParamVisitor<'a, '_> {
    fn new(params: Pin<&'a mut ffi::Parameters>) -> Self {
        Self { params, indices: KeyIndexMap::new() }
    }
}

impl<'de> sfv::visitor::ParameterVisitor<'de> for ParamVisitor<'_, 'de> {
    type Out = ();
    type Error = Infallible;

    fn parameter(
        &mut self,
        key: &'de KeyRef,
        value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        let out = match self.indices.get_or_insert(key.as_str()) {
            Some(index) => ffi::params_get(self.params.as_mut(), index),
            None => ffi::params_append(self.params.as_mut(), key.as_str()),
        };
        set_bare_item(out, value);
        Ok(())
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

impl<'de> sfv::visitor::ItemVisitor<'de> for ffi::ItemParts<'_> {
    type Out = ();
    type Error = Infallible;

    fn bare_item(
        self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl sfv::visitor::ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
        set_bare_item(self.bare_item, bare_item);
        Ok(ParamVisitor::new(self.params))
    }
}

impl<'de> sfv::visitor::InnerListVisitor<'de> for ffi::InnerListParts<'_> {
    type Error = Infallible;

    fn item(&mut self) -> Result<impl sfv::visitor::ItemVisitor<'de>, Self::Error> {
        Ok(ffi::inner_list_append_item(self.items.as_mut()))
    }

    fn finish(self) -> Result<impl sfv::visitor::ParameterVisitor<'de>, Self::Error> {
        Ok(ParamVisitor::new(self.params))
    }
}

impl<'de> sfv::visitor::EntryVisitor<'de> for Pin<&mut ffi::List> {
    type Error = Infallible;

    fn item(self) -> Result<impl sfv::visitor::ItemVisitor<'de>, Self::Error> {
        Ok(ffi::list_append_item(self))
    }

    fn inner_list(self) -> Result<impl sfv::visitor::InnerListVisitor<'de>, Self::Error> {
        Ok(ffi::list_append_inner_list(self))
    }
}

impl<'de> sfv::visitor::ListVisitor<'de> for Pin<&mut ffi::List> {
    type Out = ();
    type Error = Infallible;

    fn entry(&mut self) -> Result<impl sfv::visitor::EntryVisitor<'de>, Self::Error> {
        Ok(self.as_mut())
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

struct DictionaryEntryVisitor<'a, 'de> {
    dict: Pin<&'a mut ffi::Dictionary>,
    indices: &'a mut KeyIndexMap<'de>,
    key: &'de str,
}

impl<'de> sfv::visitor::EntryVisitor<'de> for DictionaryEntryVisitor<'_, 'de> {
    type Error = Infallible;

    fn item(self) -> Result<impl sfv::visitor::ItemVisitor<'de>, Self::Error> {
        let item = match self.indices.get_or_insert(self.key) {
            Some(index) => ffi::dictionary_set_item(self.dict, index),
            None => ffi::dictionary_append_item(self.dict, self.key),
        };
        Ok(item)
    }

    fn inner_list(self) -> Result<impl sfv::visitor::InnerListVisitor<'de>, Self::Error> {
        let inner_list = match self.indices.get_or_insert(self.key) {
            Some(index) => ffi::dictionary_set_inner_list(self.dict, index),
            None => ffi::dictionary_append_inner_list(self.dict, self.key),
        };
        Ok(inner_list)
    }
}

struct DictVisitor<'a, 'de> {
    dict: Pin<&'a mut ffi::Dictionary>,
    indices: KeyIndexMap<'de>,
}

impl<'de> sfv::visitor::DictionaryVisitor<'de> for DictVisitor<'_, 'de> {
    type Out = ();
    type Error = Infallible;

    fn entry(
        &mut self,
        key: &'de KeyRef,
    ) -> Result<impl sfv::visitor::EntryVisitor<'de>, Self::Error> {
        Ok(DictionaryEntryVisitor {
            dict: self.dict.as_mut(),
            indices: &mut self.indices,
            key: key.as_str(),
        })
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

/// Decodes a Structured Header Item from the input bytes using RFC 8941.
///
/// Returns true if decoding was successful, and false otherwise.
/// On success, the result is stored in the provided `ItemParts`.
pub fn decode_item(input: &[u8], item: ffi::ItemParts<'_>, strict: bool) -> bool {
    sfv::Parser::new(input)
        .with_version(sfv::Version::Rfc8941)
        .with_lenient_mode(!strict)
        .parse_item_with_visitor(item)
        .is_ok()
}

/// Decodes a Structured Header List from the input bytes using RFC 8941.
///
/// Returns true if decoding was successful, and false otherwise.
/// On success, the result is stored in the provided `List`.
pub fn decode_list(input: &[u8], list: Pin<&mut ffi::List>, strict: bool) -> bool {
    sfv::Parser::new(input)
        .with_version(sfv::Version::Rfc8941)
        .with_lenient_mode(!strict)
        .parse_list_with_visitor(list)
        .is_ok()
}

/// Decodes a Structured Header Dictionary from the input bytes using RFC 8941.
///
/// Returns true if decoding was successful, and false otherwise.
/// On success, the result is stored in the provided `Dictionary`.
pub fn decode_dictionary(
    input: &[u8],
    dictionary: Pin<&mut ffi::Dictionary>,
    strict: bool,
) -> bool {
    sfv::Parser::new(input)
        .with_version(sfv::Version::Rfc8941)
        .with_lenient_mode(!strict)
        .parse_dictionary_with_visitor(DictVisitor {
            dict: dictionary,
            indices: KeyIndexMap::new(),
        })
        .is_ok()
}
