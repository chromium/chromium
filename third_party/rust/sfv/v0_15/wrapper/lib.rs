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

        type Dictionary;
        type List;
        type Member;
        type Parameters;

        fn list_append_member(ctx: Pin<&mut List>) -> Pin<&mut Member>;

        fn dictionary_reset_key<'a>(ctx: Pin<&'a mut Dictionary>, key: &str)
            -> Pin<&'a mut Member>;

        fn set_member_boolean(ctx: Pin<&mut Member>, val: bool);
        fn set_member_integer(ctx: Pin<&mut Member>, val: i64);
        fn set_member_decimal(ctx: Pin<&mut Member>, val: f64);
        fn set_member_string(ctx: Pin<&mut Member>, val: &str);
        fn set_member_token(ctx: Pin<&mut Member>, val: &str);
        fn set_member_byte_sequence(ctx: Pin<&mut Member>, val: &[u8]);

        fn set_member_inner_list(ctx: Pin<&mut Member>);

        fn get_member_params(ctx: Pin<&mut Member>) -> Pin<&mut Parameters>;
        fn get_item_params(ctx: Pin<&mut Member>) -> Pin<&mut Parameters>;

        fn set_parameter_boolean(ctx: Pin<&mut Parameters>, key: &str, val: bool);
        fn set_parameter_integer(ctx: Pin<&mut Parameters>, key: &str, val: i64);
        fn set_parameter_decimal(ctx: Pin<&mut Parameters>, key: &str, val: f64);
        fn set_parameter_string(ctx: Pin<&mut Parameters>, key: &str, val: &str);
        fn set_parameter_token(ctx: Pin<&mut Parameters>, key: &str, val: &str);
        fn set_parameter_byte_sequence(ctx: Pin<&mut Parameters>, key: &str, val: &[u8]);
    }

    extern "Rust" {
        fn decode_item(input: &[u8], ctx: Pin<&mut Member>) -> bool;

        fn decode_list(input: &[u8], ctx: Pin<&mut List>) -> bool;

        fn decode_dictionary(input: &[u8], ctx: Pin<&mut Dictionary>) -> bool;
    }
}

pub type Dictionary = ffi::Dictionary;
pub type List = ffi::List;
pub type Member = ffi::Member;
pub type Parameters = ffi::Parameters;

struct MemberVisitor<'a> {
    member: Pin<&'a mut Member>,
    is_inner: bool,
}

impl<'de> sfv::visitor::ItemVisitor<'de> for MemberVisitor<'_> {
    type Out = ();
    type Error = Infallible;

    fn bare_item(
        mut self,
        bare_item: BareItemFromInput<'de>,
    ) -> Result<impl sfv::visitor::ParameterVisitor<'de, Out = Self::Out>, Self::Error> {
        match bare_item {
            BareItemFromInput::Decimal(v) => {
                ffi::set_member_decimal(self.member.as_mut(), f64::from(v))
            }
            BareItemFromInput::Integer(v) => {
                ffi::set_member_integer(self.member.as_mut(), i64::from(v))
            }
            BareItemFromInput::String(ref v) => {
                ffi::set_member_string(self.member.as_mut(), v.as_str())
            }
            BareItemFromInput::ByteSequence(ref v) => {
                ffi::set_member_byte_sequence(self.member.as_mut(), v)
            }
            BareItemFromInput::Boolean(v) => ffi::set_member_boolean(self.member.as_mut(), v),
            BareItemFromInput::Token(v) => ffi::set_member_token(self.member.as_mut(), v.as_str()),
            // RFC 8941 does not define these types; only RFC 9651 does.
            BareItemFromInput::Date(_) | BareItemFromInput::DisplayString(_) => unreachable!(),
        };
        let params = if self.is_inner {
            ffi::get_item_params(self.member)
        } else {
            ffi::get_member_params(self.member)
        };
        Ok(ParameterVisitor { params })
    }
}

impl<'de> sfv::visitor::EntryVisitor<'de> for MemberVisitor<'_> {
    type Error = Infallible;

    fn item(self) -> Result<impl sfv::visitor::ItemVisitor<'de>, Self::Error> {
        Ok(self)
    }

    fn inner_list(mut self) -> Result<impl sfv::visitor::InnerListVisitor<'de>, Self::Error> {
        ffi::set_member_inner_list(self.member.as_mut());
        Ok(self)
    }
}

impl<'de> sfv::visitor::InnerListVisitor<'de> for MemberVisitor<'_> {
    type Error = Infallible;

    fn item(&mut self) -> Result<impl sfv::visitor::ItemVisitor<'de>, Self::Error> {
        Ok(MemberVisitor { member: self.member.as_mut(), is_inner: true })
    }

    fn finish(self) -> Result<impl sfv::visitor::ParameterVisitor<'de>, Self::Error> {
        let params = ffi::get_member_params(self.member);
        Ok(ParameterVisitor { params })
    }
}

struct ParameterVisitor<'a> {
    params: Pin<&'a mut Parameters>,
}

impl<'de> sfv::visitor::ParameterVisitor<'de> for ParameterVisitor<'_> {
    type Out = ();
    type Error = Infallible;

    fn parameter(
        &mut self,
        key: &'de KeyRef,
        value: BareItemFromInput<'de>,
    ) -> Result<(), Self::Error> {
        match value {
            BareItemFromInput::Decimal(v) => {
                ffi::set_parameter_decimal(self.params.as_mut(), key.as_str(), f64::from(v))
            }
            BareItemFromInput::Integer(v) => {
                ffi::set_parameter_integer(self.params.as_mut(), key.as_str(), i64::from(v))
            }
            BareItemFromInput::String(ref v) => {
                ffi::set_parameter_string(self.params.as_mut(), key.as_str(), v.as_str())
            }
            BareItemFromInput::ByteSequence(ref v) => {
                ffi::set_parameter_byte_sequence(self.params.as_mut(), key.as_str(), v)
            }
            BareItemFromInput::Boolean(v) => {
                ffi::set_parameter_boolean(self.params.as_mut(), key.as_str(), v)
            }
            BareItemFromInput::Token(v) => {
                ffi::set_parameter_token(self.params.as_mut(), key.as_str(), v.as_str())
            }
            BareItemFromInput::Date(_) | BareItemFromInput::DisplayString(_) => unreachable!(),
        }
        Ok(())
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

struct ListVisitor<'a> {
    list: Pin<&'a mut List>,
}

impl<'de> sfv::visitor::ListVisitor<'de> for ListVisitor<'_> {
    type Out = ();
    type Error = Infallible;

    fn entry(&mut self) -> Result<impl sfv::visitor::EntryVisitor<'de>, Self::Error> {
        let member = ffi::list_append_member(self.list.as_mut());
        Ok(MemberVisitor { member, is_inner: false })
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

struct DictionaryVisitor<'a> {
    dictionary: Pin<&'a mut Dictionary>,
}

impl<'de> sfv::visitor::DictionaryVisitor<'de> for DictionaryVisitor<'_> {
    type Out = ();
    type Error = Infallible;

    fn entry(
        &mut self,
        key: &'de KeyRef,
    ) -> Result<impl sfv::visitor::EntryVisitor<'de>, Self::Error> {
        let member = ffi::dictionary_reset_key(self.dictionary.as_mut(), key.as_str());
        Ok(MemberVisitor { member, is_inner: false })
    }

    fn finish(self) -> Result<Self::Out, Self::Error> {
        Ok(())
    }
}

/// Decodes a Structured Header Item from the input bytes using RFC 8941.
///
/// Returns true if decoding was successful, and false otherwise.
/// On success, the result is stored in the provided `Member`.
pub fn decode_item(input: &[u8], member: Pin<&mut Member>) -> bool {
    sfv::Parser::new(input)
        .with_version(sfv::Version::Rfc8941)
        .with_lenient_mode(true)
        .parse_item_with_visitor(MemberVisitor { member, is_inner: false })
        .is_ok()
}

/// Decodes a Structured Header List from the input bytes using RFC 8941.
///
/// Returns true if decoding was successful, and false otherwise.
/// On success, the result is stored in the provided `List`.
pub fn decode_list(input: &[u8], list: Pin<&mut List>) -> bool {
    sfv::Parser::new(input)
        .with_version(sfv::Version::Rfc8941)
        .with_lenient_mode(true)
        .parse_list_with_visitor(ListVisitor { list })
        .is_ok()
}

/// Decodes a Structured Header Dictionary from the input bytes using RFC 8941.
///
/// Returns true if decoding was successful, and false otherwise.
/// On success, the result is stored in the provided `Dictionary`.
pub fn decode_dictionary(input: &[u8], dictionary: Pin<&mut Dictionary>) -> bool {
    sfv::Parser::new(input)
        .with_version(sfv::Version::Rfc8941)
        .with_lenient_mode(true)
        .parse_dictionary_with_visitor(DictionaryVisitor { dictionary })
        .is_ok()
}
