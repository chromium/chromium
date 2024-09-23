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

pub const BIT_STRING: u8 = 3;
pub const BOOLEAN: u8 = 1;
pub const CONSTRUCTED: u8 = 0x20;
pub const CONTEXT_SPECIFIC: u8 = 0x80;
pub const OCTET_STRING: u8 = 4;
pub const INTEGER: u8 = 2;
pub const NONE: u8 = 5;
pub const OBJECT_IDENTIFIER: u8 = 6;
pub const SEQUENCE: u8 = 0x30;
pub const UTC_TIME: u8 = 0x17;
pub const GENERALIZED_TIME: u8 = 0x18;

/// Get a byte from the front of a slice.
///
/// (This is the same as `split_first`, but returns the value rather than a
/// reference to it.)
pub fn u8_next(input: &[u8]) -> Option<(u8, &[u8])> {
    let (first, rest) = input.split_first()?;
    Some((*first, rest))
}

/// Treats the input as a series of big-endian bytes and returns a usize of
/// them. Since usize might only be 32 bits, the input must not be longer than
/// four bytes.
fn bytes_to_usize(bytes: &[u8]) -> usize {
    assert!(bytes.len() <= 4);
    let mut ret = 0usize;

    for &byte in bytes {
        ret <<= 8;
        ret |= byte as usize
    }
    ret
}

/// Returns the tag, header length, and full element (including header) of the
/// next ASN.1 DER element from `input`, as well as the remainder of the
/// input.
pub fn next_element(orig_input: &[u8]) -> Option<(u8, usize, &[u8], &[u8])> {
    let (tag_byte, input) = u8_next(orig_input)?;
    if tag_byte & 0x1f == 0x1f {
        // large-format tags are not supported.
        return None;
    }
    let (length_byte, input) = u8_next(input)?;

    let (payload_length, header_length) = if (length_byte & 0x80) == 0 {
        (length_byte as usize, 2)
    } else {
        // The high bit indicate that this is the long form, while the next 7 bits
        // encode the number of subsequent octets used to encode the length (ITU-T
        // X.690 clause 8.1.3.5.b).
        let num_bytes = (length_byte & 0x7f) as usize;
        if num_bytes == 0 || num_bytes > 4 {
            return None;
        }
        if input.len() < num_bytes {
            return None;
        }
        let (length_bytes, _rest) = input.split_at(num_bytes);
        let length = bytes_to_usize(length_bytes);
        if length < 128 {
            // Should have used short-form encoding.
            return None;
        }
        if length >> ((num_bytes - 1) * 8) == 0 {
            // Should have used fewer bytes to encode the length.
            return None;
        }
        (length, 2 + num_bytes)
    };

    let element_length = header_length.checked_add(payload_length)?;
    if orig_input.len() < element_length {
        return None;
    }
    let (element, rest) = orig_input.split_at(element_length);
    Some((tag_byte, header_length, element, rest))
}

/// Returns the tag and payload of the next ASN.1 DER element from `input`, as
/// well as the remainder of the input.
pub fn next(input: &[u8]) -> Option<(u8, &[u8], &[u8])> {
    let (tag, header_length, element, rest) = next_element(input)?;
    let (_header, payload) = element.split_at(header_length);
    Some((tag, payload, rest))
}

/// Returns the payload of the next ASN.1 DER element from `input`, and the
/// remaining input, if the element has the expected tag. Otherwise `None`.
pub fn next_tagged(input: &[u8], expected_tag: u8) -> Option<(&[u8], &[u8])> {
    let (tag, element, rest) = next(input)?;
    if tag == expected_tag { Some((element, rest)) } else { None }
}

/// Returns the body of the next ASN.1 DER element, and the remainder, from
/// `input` if it's tag is as expected. Otherwise it returns `None` and the
/// original input.
pub fn next_optional(input: &[u8], expected_tag: u8) -> Option<(Option<&[u8]>, &[u8])> {
    if input.is_empty() {
        return Some((None, input));
    }
    let (tag, body, rest) = next(input)?;
    if expected_tag == tag { Some((Some(body), rest)) } else { Some((None, input)) }
}
