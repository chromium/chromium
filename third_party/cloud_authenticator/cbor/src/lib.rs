// Copyright 2024 Google LLC
//
// Licensed under the Apache License, Version 2.0 (the "License");
// you may not use this file except in compliance with the License.
// You may obtain a copy of the License at
//
//     https://www.apache.org/licenses/LICENSE-2.0
//
// Unless required by applicable law or agreed to in writing, software
// distributed under the License is distributed on an "AS IS" BASIS,
// WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
// See the License for the specific language governing permissions and
// limitations under the License.

//! `cbor` implements the subset of CBOR used in [CTAP2][1].
//!
//! Parsing is injective, meaning that no two distinct byte strings (that parse
//! successfully) will result in the same value. Reserialising the result of
//! parse will always result in exactly the same byte string. (This is checked
//! by fuzzing.)
//!
//! In the other direction, serialisation is also injective. However, an
//! arbitary limit is placed on the maximum depth of parsed structures to avoid
//! degenerate inputs from consuming too much stack. Thus structures that are
//! deeper than this will serialise but that result cannot be parsed back to
//! the same value. Aside from this exception, all values should round-trip
//! correctly. This is checked with `proptest`.
//!
//! ```
//! let value = cbor::Value::String("hello".to_string());
//! let serialized = value.to_bytes();
//! assert_eq!(serialized, vec![0x65u8, 0x68, 0x65, 0x6c, 0x6c, 0x6f]);
//! assert_eq!(cbor::parse(serialized), Ok(value));
//! ```
//!
//! [1]: https://fidoalliance.org/specs/fido-v2.2-rd-20230321/fido-client-to-authenticator-protocol-v2.2-rd-20230321.html#ctap2-canonical-cbor-encoding-form

#![no_std]
#![forbid(unsafe_code)]

extern crate alloc;
extern crate bytes;

use alloc::collections::BTreeMap;
use alloc::string::String;
use alloc::vec::Vec;
use bytes::{Buf, Bytes};
use core::borrow::Borrow;
use core::cmp::Ordering;
use core::fmt;
use core::ops::Deref;

// This code assumes that `usize` fits in a `u64` because it uses `as u64` in a
// couple of places.
const _: () =
    assert!(core::mem::size_of::<usize>() <= core::mem::size_of::<u64>(), "usize too large");

/// MAX_DEPTH is the maximum "depth" of a structure that will be parsed.
/// Each array or map increases the depth by one.
const MAX_DEPTH: usize = 16;

/// Error enumerates the different errors that can occur during parsing.
///
/// It is intended for debugging only. Where `usize` values are present, they
/// contain the approximate number of bytes remaining when the error occurred.
#[derive(Debug, PartialEq)]
pub enum Error {
    DepthLimitExceeded(usize, usize),
    InputTruncated,
    InvalidUTF8(usize),
    MapKeysOutOfOrder(usize, Value),
    NegativeOutOfRange(u64),
    NonMinimalAdditionalData(usize),
    TrailingData(usize),
    UnsignedOutOfRange(u64),
    UnsupportedAdditionalInformation(usize, u8),
    UnsupportedMajorType(usize, u8),
    UnsupportedMapKeyType(usize, Value),
    UnsupportedSimpleValue(u64),
}

fn get_u8(bytes: &mut Bytes) -> Result<u8, Error> {
    if bytes.is_empty() {
        return Err(Error::InputTruncated);
    }

    Ok(bytes.get_u8())
}

fn get(bytes: &mut Bytes, num_bytes: usize) -> Result<Bytes, Error> {
    if bytes.len() < num_bytes {
        return Err(Error::InputTruncated);
    }

    Ok(bytes.split_to(num_bytes))
}

/// Value represents a CBOR structure.
///
/// Integers are mapped to `i64` despite CBOR having 65-bit integers. CBOR
/// integers outside the range of an `i64` result in an error during parsing.
/// Byte strings are returned as `Bytes`s in order to avoid copies.
#[derive(PartialEq, Clone)]
pub enum Value {
    Int(i64),
    Bytestring(Bytes),
    String(String),
    Array(Vec<Value>),
    Map(BTreeMap<MapKey, Value>),
    Boolean(bool),
}

/// Implements pretty-printing of `Value`s and `MapKey`s to better support the
/// `Debug` trait for those types.
mod debug {
    use super::{MapKey, Value};
    use alloc::string::String;
    use core::fmt;

    const HEX_CHARS: [char; 16] =
        ['0', '1', '2', '3', '4', '5', '6', '7', '8', '9', 'a', 'b', 'c', 'd', 'e', 'f'];

    fn hex_encode(bytes: &[u8]) -> String {
        let mut ret = String::with_capacity(bytes.len() * 2);
        for &byte in bytes {
            ret.push(HEX_CHARS[(byte >> 4) as usize]);
            ret.push(HEX_CHARS[(byte & 15) as usize]);
        }
        ret
    }

    /// Write a debugging representation of `key` to `f`.
    pub fn map_key(f: &mut fmt::Formatter, key: &MapKey) -> fmt::Result {
        match key {
            MapKey::Int(i) => write!(f, "{}", i),
            MapKey::Bytestring(bytes) => {
                f.write_str("h\"")?;
                f.write_str(&hex_encode(bytes))?;
                f.write_str("\"")
            }
            MapKey::String(s) => {
                f.write_str("\"")?;
                f.write_str(s)?;
                f.write_str("\"")
            }
        }
    }

    /// Write a debugging representation of `value` to `f`.
    pub fn value(f: &mut fmt::Formatter, value: &Value) -> fmt::Result {
        match value {
            Value::Int(i) => write!(f, "{}", i),
            Value::Bytestring(bytes) => {
                f.write_str("h\"")?;
                f.write_str(&hex_encode(bytes))?;
                f.write_str("\"")
            }
            Value::String(s) => {
                f.write_str("\"")?;
                f.write_str(s)?;
                f.write_str("\"")
            }
            Value::Boolean(b) => f.write_str(if *b { "true" } else { "false" }),
            Value::Array(array) => f.debug_list().entries(array.iter()).finish(),
            Value::Map(map) => f.debug_map().entries(map.iter()).finish(),
        }
    }
}

impl fmt::Debug for MapKey {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        debug::map_key(f, self)
    }
}

impl fmt::Debug for Value {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        debug::value(f, self)
    }
}

// low_bits_and_length returns the bottom five bits of the initial byte of a
// CBOR value, and the number of bytes that will need to follow it in order to
// encode an argument `arg`.
fn low_bits_and_length(arg: u64) -> (u8, usize) {
    if arg < 24 {
        (arg as u8, 0)
    } else if arg < 0x100 {
        (24, 1)
    } else if arg < 0x10000 {
        (25, 2)
    } else if arg < 0x100000000 {
        (26, 4)
    } else {
        (27, 8)
    }
}

/// write_header appends the [initial byte and "argument"][1] of a CBOR value.
///
/// [1]: https://datatracker.ietf.org/doc/html/rfc8949#name-specification-of-the-cbor-e
fn write_header(out: &mut Vec<u8>, major_type: u8, arg: u64) {
    let (low_bits, num_bytes) = low_bits_and_length(arg);
    out.push(major_type << 5 | low_bits);
    let value_bytes = arg.to_be_bytes();
    out.extend_from_slice(&value_bytes[8 - num_bytes..]);
}

impl Value {
    // to_bytes serialises `self` to CBOR and returns the result.
    pub fn to_bytes(&self) -> Vec<u8> {
        let mut ret = Vec::new();
        self.append_bytes(&mut ret);
        ret
    }

    // append_bytes appends a serialisation of `self` to `out`.
    pub fn append_bytes(&self, out: &mut Vec<u8>) {
        match self {
            Value::Int(v) if v >= &0 => write_header(out, 0, *v as u64),
            Value::Int(v) => write_header(out, 1, !v as u64),
            Value::Bytestring(s) => {
                write_header(out, 2, s.len() as u64);
                out.extend_from_slice(s.deref());
            }
            Value::String(s) => {
                write_header(out, 3, s.len() as u64);
                out.extend_from_slice(s.as_bytes());
            }
            Value::Array(a) => {
                write_header(out, 4, a.len() as u64);
                for elem in a {
                    elem.append_bytes(out);
                }
            }
            Value::Map(m) => {
                write_header(out, 5, m.len() as u64);
                for (key, value) in m {
                    key.append_bytes(out);
                    value.append_bytes(out);
                }
            }
            Value::Boolean(b) => write_header(out, 7, 20u64 + *b as u64),
        }
    }
}

impl From<&str> for Value {
    fn from(s: &str) -> Self {
        Value::String(String::from(s))
    }
}

impl From<i64> for Value {
    fn from(i: i64) -> Self {
        Value::Int(i)
    }
}

impl From<bool> for Value {
    fn from(b: bool) -> Self {
        Value::Boolean(b)
    }
}

impl From<&[u8]> for Value {
    fn from(bytes: &[u8]) -> Self {
        Value::Bytestring(Bytes::from(bytes.to_vec()))
    }
}

impl<const N: usize> From<&[u8; N]> for Value {
    fn from(bytes: &[u8; N]) -> Self {
        Value::Bytestring(Bytes::from(bytes.to_vec()))
    }
}

impl From<Vec<u8>> for Value {
    fn from(bytes: Vec<u8>) -> Self {
        Value::Bytestring(Bytes::from(bytes))
    }
}

impl From<Vec<Value>> for Value {
    fn from(values: Vec<Value>) -> Self {
        Value::Array(values)
    }
}

impl From<BTreeMap<MapKey, Value>> for Value {
    fn from(map: BTreeMap<MapKey, Value>) -> Self {
        Value::Map(map)
    }
}

/// The `cbor!` macro allows `Value`s to be constructed with less syntax noise.
/// For example:
///
/// ```ignore
/// cbor!({
///   "key": "value",
///   "key2": [1, 2, 3],
///   "key3": {
///     "nested": true,
///   },
///   1: 2,
///   3: true,
/// });
/// ```
///
/// Because of the way that the macro is implemented, when using values
/// (including variable names) that consist of more than one token they will
/// need to be wrapped in parentheses.
#[macro_export]
macro_rules! cbor {
    ([ $( $elem:tt ),* ]) => {
        $crate::Value::Array(vec![ $( $crate::cbor!($elem) ),* ])
    };
    ([ $( $elem:tt ),* ,]) => {
        $crate::cbor!([ $( $elem ),* ])
    };
    ({ $( $key:tt : $value:tt ),* }) => {{
        let mut map = alloc::collections::BTreeMap::new();
        $( map.insert($crate::MapKey::from($key), $crate::cbor!($value)); )*
        $crate::Value::Map(map)
    }};
    ({ $( $key:tt : $value:tt ),* ,}) => {
        $crate::cbor!({ $( $key : $value ),* })
    };
    ( $value:tt ) => { $crate::Value::from($value) };
}

/// A MapKey is the type of values that can key a CBOR map.
#[derive(PartialEq, Eq, Clone)]
pub enum MapKey {
    // A separate `MapKey` type is used because we want to exclude things like
    // maps keyed by arrays or other maps. Such structures never appear in
    // CTAP and so we don't need to support them.
    //
    // We expect that a map will always have keys of the same type, which
    // suggests that `Value::Map` could be split into `Value::IntKeyedMap` etc.
    // However, that falls down when a map is empty because the parser can't
    // know what the key type should be, yet calling code will want to expect
    // the right type of map. Thus we end up supporting heterogeneous maps.
    Int(i64),
    Bytestring(Vec<u8>),
    String(String),
}

impl MapKey {
    fn as_ref(&self) -> MapKeyRef {
        match self {
            MapKey::Int(v) => MapKeyRef::Int(*v),
            MapKey::Bytestring(b) => MapKeyRef::Slice(b),
            MapKey::String(s) => MapKeyRef::Str(s),
        }
    }

    fn append_bytes(&self, out: &mut Vec<u8>) {
        self.as_ref().append_bytes(out)
    }
}

impl PartialOrd for MapKey {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for MapKey {
    fn cmp(&self, other: &Self) -> Ordering {
        // It's essential that the ordering of `MapKey` and `MapKeyRef` be
        // congrugent. To ensure this, the order of `MapKey` is delegated to
        // `MapKeyRef`.
        self.as_ref().cmp(&other.as_ref())
    }
}

impl From<&str> for MapKey {
    fn from(s: &str) -> Self {
        MapKey::String(String::from(s))
    }
}

impl From<i64> for MapKey {
    fn from(i: i64) -> Self {
        MapKey::Int(i)
    }
}

impl From<&[u8]> for MapKey {
    fn from(bytes: &[u8]) -> Self {
        MapKey::Bytestring(bytes.to_vec())
    }
}

impl<const N: usize> From<&[u8; N]> for MapKey {
    fn from(bytes: &[u8; N]) -> Self {
        MapKey::Bytestring(bytes.to_vec())
    }
}

impl From<Vec<u8>> for MapKey {
    fn from(bytes: Vec<u8>) -> Self {
        MapKey::Bytestring(bytes)
    }
}

/// MapKeyRef mirrors MapKey and allows lookups without allocation.
///
/// The `BTreeMap` in a [`Value::Map`] would normally require a [`MapKey`] as a
/// lookup key, but that may require allocating Vecs or Strings to construct.
/// Alternatively, `BTreeKey` allows lookups with any type that can be borrowed
/// from the keys, but there is not any such type for MapKey by default.
///
/// Thus MapKeyRef mirrors [`MapKey`], but uses borrowed types. Both it and
/// [`MapKey`] implement [`MapLookupKey`], and a [`MapLookupKey`] can be
/// borrowed from a [`MapKey`]. Thus, to lookup a value without allocating, do
/// something like:
///
/// ```
/// let map = std::collections::BTreeMap::from([
///   (cbor::MapKey::String(String::from("bubbles")), cbor::Value::Int(1)),
///   (cbor::MapKey::String(String::from("ducks")), cbor::Value::Int(2)),
/// ]);
/// map.get(&cbor::MapKeyRef::Str("bubbles") as &dyn cbor::MapLookupKey);
/// ```
#[derive(PartialEq, Eq, Clone)]
pub enum MapKeyRef<'a> {
    Int(i64),
    Slice(&'a [u8]),
    Str(&'a str),
}

impl MapKeyRef<'_> {
    fn type_arg_and_payload(&self) -> (u8, u64, Option<&[u8]>) {
        match self {
            MapKeyRef::Int(v) if v >= &0 => (0, *v as u64, None),
            MapKeyRef::Int(v) => (1, !v as u64, None),
            MapKeyRef::Slice(b) => (2, b.len() as u64, Some(b)),
            MapKeyRef::Str(s) => (3, s.len() as u64, Some(s.as_bytes())),
        }
    }

    fn append_bytes(&self, out: &mut Vec<u8>) {
        let (major_type, arg, payload) = self.type_arg_and_payload();
        write_header(out, major_type, arg);
        if let Some(payload) = payload {
            out.extend_from_slice(payload);
        }
    }
}

impl PartialOrd for MapKeyRef<'_> {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.cmp(other))
    }
}

impl Ord for MapKeyRef<'_> {
    fn cmp(&self, other: &Self) -> Ordering {
        self.type_arg_and_payload().cmp(&other.type_arg_and_payload())
    }
}

/// To be used with [`MapKeyRef`]. See the documentation for that type.
pub trait MapLookupKey {
    fn to_key(&self) -> MapKeyRef;
}

impl PartialEq for dyn MapLookupKey + '_ {
    fn eq(&self, other: &Self) -> bool {
        self.to_key().eq(&other.to_key())
    }
}

impl Eq for dyn MapLookupKey + '_ {}

impl PartialOrd for dyn MapLookupKey + '_ {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        Some(self.to_key().cmp(&other.to_key()))
    }
}

impl Ord for dyn MapLookupKey + '_ {
    fn cmp(&self, other: &Self) -> Ordering {
        self.to_key().cmp(&other.to_key())
    }
}

impl MapLookupKey for MapKey {
    fn to_key(&self) -> MapKeyRef {
        self.as_ref()
    }
}

impl<'a> MapLookupKey for MapKeyRef<'a> {
    fn to_key(&self) -> MapKeyRef {
        self.clone()
    }
}

impl<'a> Borrow<dyn MapLookupKey + 'a> for MapKey {
    fn borrow(&self) -> &(dyn MapLookupKey + 'a) {
        self
    }
}

/// Parses a CBOR structure from `input_vec`.
///
/// It takes ownership of the `Vec` because byte strings within are returned
/// as ref-counted references to the input. If you want to retain a copy of
/// the input, use `parse_bytes`.
pub fn parse(input_vec: Vec<u8>) -> Result<Value, Error> {
    parse_bytes(input_vec.into())
}

/// Parses a CBOR structure from a `Slice`.
///
/// This can be used to parse CBOR within another CBOR structure without
/// having to make a copy of the intermediate byte string.
pub fn parse_bytes(mut input: Bytes) -> Result<Value, Error> {
    let ret = parse_value(&mut input, 0)?;
    if !input.is_empty() { Err(Error::TrailingData(input.len())) } else { Ok(ret) }
}

fn parse_value(input: &mut Bytes, depth: usize) -> Result<Value, Error> {
    if depth > MAX_DEPTH {
        return Err(Error::DepthLimitExceeded(input.len(), MAX_DEPTH));
    }
    let (major_type, arg) = parse_header(input)?;
    match major_type {
        0 => to_int(arg, false),
        1 => to_int(arg, true),
        2 => to_bytestring(input, arg),
        3 => to_string(input, arg),
        4 => to_array(input, arg, depth + 1),
        5 => to_map(input, arg, depth + 1),
        7 => to_boolean(arg),
        _ => Result::Err(Error::UnsupportedMajorType(input.len(), major_type)),
    }
}

fn parse_header(input: &mut Bytes) -> Result<(u8, u64), Error> {
    let b = get_u8(input)?;
    let major_type = b >> 5;
    let info = b & 0x1f;
    let arg = (match info {
        0..=23 => Ok(info as u64),
        24 => get_argument(input, 1),
        25 => get_argument(input, 2),
        26 => get_argument(input, 4),
        27 => get_argument(input, 8),
        _ => Err(Error::UnsupportedAdditionalInformation(input.len(), info)),
    })?;
    Ok((major_type, arg))
}

fn get_argument(input: &mut Bytes, num_bytes: u8) -> Result<u64, Error> {
    let mut v: u64 = 0;
    for _ in 0..num_bytes {
        v <<= 8;
        let b = get_u8(input)?;
        v |= b as u64;
    }
    let (_, expected_num_bytes) = low_bits_and_length(v);
    if num_bytes as usize != expected_num_bytes {
        Err(Error::NonMinimalAdditionalData(input.len()))
    } else {
        Ok(v)
    }
}

fn to_int(arg: u64, is_negative: bool) -> Result<Value, Error> {
    if is_negative {
        if arg > i64::MAX as u64 {
            Err(Error::NegativeOutOfRange(arg))
        } else {
            Ok(Value::Int(!arg as i64))
        }
    } else if arg > i64::MAX as u64 {
        Err(Error::UnsignedOutOfRange(arg))
    } else {
        Ok(Value::Int(arg as i64))
    }
}

fn to_bytestring(input: &mut Bytes, len64: u64) -> Result<Value, Error> {
    let Some(len): Option<usize> = len64.try_into().ok() else {
        return Err(Error::InputTruncated);
    };
    let bytes = get(input, len)?;
    Ok(Value::Bytestring(bytes))
}

fn to_string(input: &mut Bytes, len64: u64) -> Result<Value, Error> {
    let Some(len): Option<usize> = len64.try_into().ok() else {
        return Err(Error::InputTruncated);
    };
    let orig_len = input.len();
    let bytes = get(input, len)?;
    let vec = Vec::from(bytes.deref());
    let Some(string) = String::from_utf8(vec).ok() else {
        return Err(Error::InvalidUTF8(orig_len));
    };
    Ok(Value::String(string))
}

fn to_array(input: &mut Bytes, num_elements: u64, depth: usize) -> Result<Value, Error> {
    let mut ret = Vec::new();
    for _ in 0..num_elements {
        ret.push(parse_value(input, depth)?);
    }
    Ok(Value::Array(ret))
}

fn to_map(input: &mut Bytes, num_elements: u64, depth: usize) -> Result<Value, Error> {
    let mut ret = BTreeMap::new();
    let mut previous_key: Option<Bytes> = Option::None;

    for _ in 0..num_elements {
        let mut key_slice = input.clone();
        let key_value = parse_value(input, depth)?;
        key_slice.truncate(key_slice.len() - input.len());
        if let Some(previous_key_slice) = previous_key {
            if *key_slice <= *previous_key_slice {
                return Err(Error::MapKeysOutOfOrder(input.len(), key_value));
            }
        }
        previous_key = Some(key_slice);

        let key = match key_value {
            Value::Int(i) => MapKey::Int(i),
            Value::Bytestring(b) => MapKey::Bytestring(Vec::from(b.deref())),
            Value::String(s) => MapKey::String(s),
            _ => return Err(Error::UnsupportedMapKeyType(input.len(), key_value)),
        };
        let value = parse_value(input, depth)?;
        ret.insert(key, value);
    }
    Ok(Value::Map(ret))
}

fn to_boolean(arg: u64) -> Result<Value, Error> {
    if arg == 20 {
        Ok(Value::Boolean(false))
    } else if arg == 21 {
        Ok(Value::Boolean(true))
    } else {
        Err(Error::UnsupportedSimpleValue(arg))
    }
}

#[cfg(test)]
mod tests {
    extern crate hex;
    use super::*;
    use alloc::{format, vec};
    use proptest::prelude::*;

    #[test]
    fn test_inputs() {
        let test_cases = [
            ("", Err(Error::InputTruncated)),
            ("d0", Err(Error::UnsupportedMajorType(0, 6))),
            ("1f", Err(Error::UnsupportedAdditionalInformation(0, 31))),
            ("01", Ok(Value::Int(1))),
            ("0100", Err(Error::TrailingData(1))),
            ("20", Ok(Value::Int(-1))),
            ("182a", Ok(Value::Int(42))),
            ("191388", Ok(Value::Int(5000))),
            ("1a02faf080", Ok(Value::Int(50000000))),
            ("1b000000746a528800", Ok(Value::Int(500000000000))),
            ("1b000000746a528800", Ok(Value::Int(500000000000))),
            ("1bffffffffffffffff", Err(Error::UnsignedOutOfRange(0xffffffffffffffff))),
            ("3bffffffffffffffff", Err(Error::NegativeOutOfRange(0xffffffffffffffff))),
            ("1818", Ok(Value::Int(24))),
            ("1817", Err(Error::NonMinimalAdditionalData(0))),
            ("190080", Err(Error::NonMinimalAdditionalData(0))),
            ("60", Ok(Value::String(String::from("")))),
            ("6161", Ok(Value::String(String::from("a")))),
            ("40", Ok(Value::Bytestring(Bytes::new()))),
            ("4100", Ok(Value::Bytestring(Bytes::from(b"\x00".as_slice())))),
            ("61ff", Err(Error::InvalidUTF8(1))),
            ("80", Ok(Value::Array(Vec::new()))),
            ("8101", Ok(Value::Array(vec![Value::Int(1)]))),
            ("818101", Ok(Value::Array(vec![Value::Array(vec![Value::Int(1)])]))),
            (
                "8181818181818181818181818181818181818180",
                Err(Error::DepthLimitExceeded(3, MAX_DEPTH)),
            ),
            ("a0", Ok(Value::Map(BTreeMap::new()))),
            ("a10101", Ok(Value::Map(BTreeMap::from([(MapKey::Int(1), Value::Int(1))])))),
            ("a12101", Ok(Value::Map(BTreeMap::from([(MapKey::Int(-2), Value::Int(1))])))),
            (
                "a201010202",
                Ok(Value::Map(BTreeMap::from([
                    (MapKey::Int(1), Value::Int(1)),
                    (MapKey::Int(2), Value::Int(2)),
                ]))),
            ),
            (
                "a1410a01",
                Ok(Value::Map(BTreeMap::from([(
                    MapKey::Bytestring(hex::decode("0a").unwrap()),
                    Value::Int(1),
                )]))),
            ),
            // This is a COSE key to check that map ordering works correctly.
            (
                "a501020326200121582009ac4af6a4646b5bfe81c37f751769c768c5c41ffea633dad0f48e6e3bc3e9a0225820269fbe132c40bf11f4de4a92bec527901906fdce98bbed52df9b175b6a4f3808",
                Ok(Value::Map(BTreeMap::from([
                    (MapKey::Int(1), Value::Int(2)),
                    (MapKey::Int(3), Value::Int(-7)),
                    (MapKey::Int(-1), Value::Int(1)),
                    (
                        MapKey::Int(-2),
                        Value::Bytestring(Bytes::from(
                            hex::decode(
                                "09ac4af6a4646b5bfe81c37f751769c768c5c41ffea633dad0f48e6e3bc3e9a0",
                            )
                            .unwrap(),
                        )),
                    ),
                    (
                        MapKey::Int(-3),
                        Value::Bytestring(Bytes::from(
                            hex::decode(
                                "269fbe132c40bf11f4de4a92bec527901906fdce98bbed52df9b175b6a4f3808",
                            )
                            .unwrap(),
                        )),
                    ),
                ]))),
            ),
            (
                "a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a101a0",
                Err(Error::DepthLimitExceeded(6, MAX_DEPTH)),
            ),
            ("a202010102", Err(Error::MapKeysOutOfOrder(1, Value::Int(1)))),
            ("a1810101", Err(Error::UnsupportedMapKeyType(1, Value::Array(vec![Value::Int(1)])))),
            ("f4", Ok(Value::Boolean(false))),
            ("f5", Ok(Value::Boolean(true))),
            ("f6", Err(Error::UnsupportedSimpleValue(22))),
        ];

        for test in test_cases {
            let bytes = Bytes::from(hex::decode(test.0).unwrap());
            let result = parse_bytes(bytes.clone());
            assert_eq!(result, test.1, "{}", test.0);
            if let Ok(value) = result {
                let bytes2 = value.to_bytes();
                assert_eq!(bytes, bytes2, "{}", test.0);
            }
            if bytes.len() > 0 {
                // All truncations of the input should fail to parse.
                for i in 0..bytes.len() - 1 {
                    let mut bytes2 = bytes.clone();
                    bytes2.truncate(i);
                    let result = parse_bytes(bytes2.clone());
                    assert!(matches!(result, Result::Err(_)));
                }
            }
        }
    }

    const EXAMPLE: &str = "example";
    const EXAMPLE_KEY: &dyn MapLookupKey = &MapKeyRef::Str(EXAMPLE) as &dyn MapLookupKey;

    #[test]
    fn test_lookup() {
        let map = BTreeMap::from([(MapKey::String(String::from(EXAMPLE)), Value::Int(1))]);
        assert_eq!(map.get(EXAMPLE_KEY), Some(&Value::Int(1)));
    }

    #[test]
    fn test_macro() {
        assert_eq!(cbor!(1), Value::Int(1));
        assert_eq!(cbor!("test"), Value::String(String::from("test")));
        assert_eq!(cbor!(b"123"), Value::Bytestring(Bytes::from(b"\x31\x32\x33".as_slice())));
        assert_eq!(cbor!([1, 2]), Value::Array(vec![Value::Int(1), Value::Int(2)]));
        assert_eq!(
            cbor!([1, true, "str"]),
            Value::Array(vec![
                Value::Int(1),
                Value::Boolean(true),
                Value::String(String::from("str"))
            ])
        );
        assert_eq!(
            cbor!({1: 2, 3: 4}),
            Value::Map(BTreeMap::from([
                (MapKey::Int(1), Value::Int(2)),
                (MapKey::Int(3), Value::Int(4))
            ]))
        );
        assert_eq!(
            cbor!({1: 2, 3: [1]}),
            Value::Map(BTreeMap::from([
                (MapKey::Int(1), Value::Int(2)),
                (MapKey::Int(3), Value::Array(vec![Value::Int(1)]))
            ]))
        );
        assert_eq!(
            cbor!({1: 2, 3: {"s":2}}),
            Value::Map(BTreeMap::from([
                (MapKey::Int(1), Value::Int(2)),
                (
                    MapKey::Int(3),
                    Value::Map(BTreeMap::from([(
                        MapKey::String(String::from("s")),
                        Value::Int(2)
                    )]))
                )
            ]))
        );
        assert_eq!(
            cbor!({b"0": 1}),
            Value::Map(BTreeMap::from([(MapKey::Bytestring(vec![0x30u8]), Value::Int(1)),]))
        );
    }

    #[test]
    fn test_debug() {
        let value = cbor!({
            1: 2,
            "three": "four",
            "five": [6, 7, "eight"],
            "nine": {
                "ten": [11],
                "twelve": {
                    b"13": 14,
                },
            },
        });
        let debug = format!("{:?}", value);
        assert_eq!(
            debug,
            "{1: 2, \"five\": [6, 7, \"eight\"], \"nine\": {\"ten\": [11], \"twelve\": {h\"3133\": 14}}, \"three\": \"four\"}"
        );
    }

    fn arb_map_key() -> impl Strategy<Value = MapKey> {
        prop_oneof![
            any::<i64>().prop_map(MapKey::Int),
            ".*".prop_map(MapKey::String),
            prop::collection::vec(0..255u8, 0..32).prop_map(MapKey::Bytestring),
        ]
    }

    fn arb_value() -> impl Strategy<Value = Value> {
        let leaf = prop_oneof![
            any::<i64>().prop_map(Value::Int),
            prop::collection::vec(0..255u8, 0..512)
                .prop_map(Bytes::from)
                .prop_map(Value::Bytestring),
            ".*".prop_map(Value::String),
            any::<bool>().prop_map(Value::Boolean),
        ];
        leaf.prop_recursive(
            8,   // 8 levels deep
            256, // maximum size of 256 nodes
            10,  // up to 10 items per collection
            |inner| {
                prop_oneof![
                    prop::collection::vec(inner.clone(), 0..10).prop_map(Value::Array),
                    prop::collection::btree_map(arb_map_key(), inner, 0..10).prop_map(Value::Map),
                ]
            },
        )
    }

    proptest! {
        #![proptest_config(ProptestConfig::with_cases(500))]
        #[test]
        fn test_serialize(a in arb_value()) {
            assert_eq!(parse(a.to_bytes()).unwrap(), a);
        }
    }

    proptest! {
        #![proptest_config(ProptestConfig::with_cases(100))]
        #[test]
        fn test_map_key_ordering(a in arb_map_key(), b in arb_map_key()) {
            // Ordering of MapKeys should match the ordering of their
            // serialisations.
            let mut a_bytes = Vec::new();
            a.append_bytes(&mut a_bytes);
            let mut b_bytes = Vec::new();
            b.append_bytes(&mut b_bytes);
            assert_eq!(a_bytes.cmp(&b_bytes), a.cmp(&b));
        }
    }
}
