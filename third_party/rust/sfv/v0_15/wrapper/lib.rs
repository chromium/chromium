// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

use sfv::{BareItemFromInput, KeyRef};
use std::convert::Infallible;
use std::pin::Pin;

/// C++ bindings
#[cxx::bridge(namespace=sfv)]
mod ffi {
    // From the `wrapper_functions` target.
    unsafe extern "C++" {
        include!("third_party/rust/sfv/v0_15/wrapper/functions.h");

        type BareItem;
        type Dictionary;
        type InnerList;
        type Item;
        type List;

        fn list_append_item(ctx: Pin<&mut List>) -> Pin<&mut Item>;
        fn list_append_inner_list(ctx: Pin<&mut List>) -> Pin<&mut InnerList>;

        fn dictionary_set_item<'a>(ctx: Pin<&'a mut Dictionary>, key: &str) -> Pin<&'a mut Item>;
        fn dictionary_set_inner_list<'a>(
            ctx: Pin<&'a mut Dictionary>,
            key: &str,
        ) -> Pin<&'a mut InnerList>;

        fn set_bare_item_boolean(ctx: Pin<&mut BareItem>, val: bool);
        fn set_bare_item_integer(ctx: Pin<&mut BareItem>, val: i64);
        fn set_bare_item_decimal(ctx: Pin<&mut BareItem>, val: f64);
        fn set_bare_item_string(ctx: Pin<&mut BareItem>, val: &str);
        fn set_bare_item_token(ctx: Pin<&mut BareItem>, val: &str);
        fn set_bare_item_byte_sequence(ctx: Pin<&mut BareItem>, val: &[u8]);

        fn inner_list_append_item(ctx: Pin<&mut InnerList>) -> Pin<&mut Item>;
        fn get_or_insert_inner_list_param<'a>(
            ctx: Pin<&'a mut InnerList>,
            key: &str,
        ) -> Pin<&'a mut BareItem>;

        fn get_item_bare_item(ctx: Pin<&mut Item>) -> Pin<&mut BareItem>;
        fn get_or_insert_item_param<'a>(ctx: Pin<&'a mut Item>, key: &str)
            -> Pin<&'a mut BareItem>;
    }

    extern "Rust" {
        fn decode_item(input: &[u8], item: Pin<&mut Item>, strict: bool) -> bool;

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

impl<'de> sfv::visitor::ItemVisitor<'de> for Pin<&mut ffi::Item> {
    type Out = ();
    type Error = Infallible;

    fn bare_item(
        mut self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl sfv::visitor::ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
        let out = ffi::get_item_bare_item(self.as_mut());
        set_bare_item(out, bare_item);
        Ok(self)
    }
}

impl<'de> sfv::visitor::ParameterVisitor<'de> for Pin<&mut ffi::Item> {
    type Out = ();
    type Error = Infallible;

    fn parameter(
        &mut self,
        key: &'de KeyRef,
        value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        let out = ffi::get_or_insert_item_param(self.as_mut(), key.as_str());
        set_bare_item(out, value);
        Ok(())
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

impl<'de> sfv::visitor::InnerListVisitor<'de> for Pin<&mut ffi::InnerList> {
    type Error = Infallible;

    fn item(&mut self) -> Result<impl sfv::visitor::ItemVisitor<'de>, Self::Error> {
        Ok(ffi::inner_list_append_item(self.as_mut()))
    }

    fn finish(self) -> Result<impl sfv::visitor::ParameterVisitor<'de>, Self::Error> {
        Ok(self)
    }
}

impl<'de> sfv::visitor::ParameterVisitor<'de> for Pin<&mut ffi::InnerList> {
    type Out = ();
    type Error = Infallible;

    fn parameter(
        &mut self,
        key: &'de KeyRef,
        value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        let out = ffi::get_or_insert_inner_list_param(self.as_mut(), key.as_str());
        set_bare_item(out, value);
        Ok(())
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
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

struct DictionaryVisitor<'a, 'de> {
    dict: Pin<&'a mut ffi::Dictionary>,
    key: &'de str,
}

impl<'de> sfv::visitor::EntryVisitor<'de> for DictionaryVisitor<'_, '_> {
    type Error = Infallible;

    fn item(self) -> Result<impl sfv::visitor::ItemVisitor<'de>, Self::Error> {
        Ok(ffi::dictionary_set_item(self.dict, self.key))
    }

    fn inner_list(self) -> Result<impl sfv::visitor::InnerListVisitor<'de>, Self::Error> {
        Ok(ffi::dictionary_set_inner_list(self.dict, self.key))
    }
}

impl<'de> sfv::visitor::DictionaryVisitor<'de> for Pin<&mut ffi::Dictionary> {
    type Out = ();
    type Error = Infallible;

    fn entry(
        &mut self,
        key: &'de KeyRef,
    ) -> Result<impl sfv::visitor::EntryVisitor<'de>, Self::Error> {
        Ok(DictionaryVisitor { dict: self.as_mut(), key: key.as_str() })
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

/// Decodes a Structured Header Item from the input bytes using RFC 8941.
///
/// Returns true if decoding was successful, and false otherwise.
/// On success, the result is stored in the provided `Item`.
pub fn decode_item(input: &[u8], item: Pin<&mut ffi::Item>, strict: bool) -> bool {
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
        .parse_dictionary_with_visitor(dictionary)
        .is_ok()
}
