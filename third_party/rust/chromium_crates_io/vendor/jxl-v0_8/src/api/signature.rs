// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

/// The magic bytes for a bare JPEG XL codestream.
const CODESTREAM_SIGNATURE: [u8; 2] = [0xff, 0x0a];
/// The magic bytes for a file using the JPEG XL container format.
const CONTAINER_SIGNATURE: [u8; 12] = [0, 0, 0, 0xc, b'J', b'X', b'L', b' ', 0xd, 0xa, 0x87, 0xa];

#[derive(Debug, PartialEq, Eq, Clone, Copy)]
pub enum JxlSignature {
    Codestream,
    Container,
    None,
    NeedMoreInput { size_hint: usize },
}

impl JxlSignature {
    pub(crate) fn signature_len(&self) -> usize {
        match self {
            JxlSignature::Container => CONTAINER_SIGNATURE.len(),
            JxlSignature::Codestream => CODESTREAM_SIGNATURE.len(),
            _ => 0,
        }
    }
}

/// Checks if the given buffer starts with a valid JPEG XL signature.
///
/// # Returns
///
/// A [`JxlSignature`] which is:
/// - `Codestream | Container` if a full container or codestream signature is found.
/// - `None` if the prefix is definitively not a JXL signature.
/// - `NeedMoreInput` if the prefix matches a signature but is too short.
pub fn check_signature(file_prefix: &[u8]) -> JxlSignature {
    let prefix_len = file_prefix.len();

    for (ret, sign) in [
        (JxlSignature::Codestream, &CODESTREAM_SIGNATURE[..]),
        (JxlSignature::Container, &CONTAINER_SIGNATURE),
    ] {
        let len = sign.len();
        // Determine the number of bytes to compare (the length of the shorter slice)
        let len_to_check = prefix_len.min(len);

        if file_prefix[..len_to_check] == sign[..len_to_check] {
            // The prefix is a valid start. Now, is it complete?
            return if prefix_len >= len {
                ret
            } else {
                JxlSignature::NeedMoreInput {
                    size_hint: len - prefix_len,
                }
            };
        }
    }
    // The prefix doesn't match the start of any known signature.
    JxlSignature::None
}
