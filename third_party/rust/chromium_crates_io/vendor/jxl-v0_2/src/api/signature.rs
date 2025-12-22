// Copyright (c) the JPEG XL Project Authors. All rights reserved.
//
// Use of this source code is governed by a BSD-style
// license that can be found in the LICENSE file.

use crate::{
    api::ProcessingResult,
    error::{Error, Result},
};

/// The magic bytes for a bare JPEG XL codestream.
pub(crate) const CODESTREAM_SIGNATURE: [u8; 2] = [0xff, 0x0a];
/// The magic bytes for a file using the JPEG XL container format.
pub(crate) const CONTAINER_SIGNATURE: [u8; 12] =
    [0, 0, 0, 0xc, b'J', b'X', b'L', b' ', 0xd, 0xa, 0x87, 0xa];

#[derive(Debug, PartialEq)]
pub enum JxlSignatureType {
    Codestream,
    Container,
}

impl JxlSignatureType {
    pub(crate) fn signature(&self) -> &[u8] {
        match self {
            JxlSignatureType::Container => &CONTAINER_SIGNATURE,
            JxlSignatureType::Codestream => &CODESTREAM_SIGNATURE,
        }
    }
}

pub(crate) fn check_signature_internal(file_prefix: &[u8]) -> Result<Option<JxlSignatureType>> {
    let prefix_len = file_prefix.len();

    for st in [JxlSignatureType::Codestream, JxlSignatureType::Container] {
        let len = st.signature().len();
        // Determine the number of bytes to compare (the length of the shorter slice)
        let len_to_check = prefix_len.min(len);

        if file_prefix[..len_to_check] == st.signature()[..len_to_check] {
            // The prefix is a valid start. Now, is it complete?
            return if prefix_len >= len {
                Ok(Some(st))
            } else {
                Err(Error::OutOfBounds(len - prefix_len))
            };
        }
    }
    // The prefix doesn't match the start of any known signature.
    Ok(None)
}

/// Checks if the given buffer starts with a valid JPEG XL signature.
///
/// # Returns
///
/// A `ProcessingResult` which is:
/// - `Complete(Some(_))` if a full container or codestream signature is found.
/// - `Complete(None)` if the prefix is definitively not a JXL signature.
/// - `NeedsMoreInput` if the prefix matches a signature but is too short.
pub fn check_signature(file_prefix: &[u8]) -> ProcessingResult<Option<JxlSignatureType>, ()> {
    ProcessingResult::new(check_signature_internal(file_prefix)).unwrap()
}

#[cfg(test)]
mod tests {
    use crate::api::{
        CODESTREAM_SIGNATURE, CONTAINER_SIGNATURE, JxlSignatureType, ProcessingResult,
        check_signature,
    };

    macro_rules! signature_test {
        ($test_name:ident, $bytes:expr, Complete(Some($expected_type:expr))) => {
            #[test]
            fn $test_name() {
                let result = check_signature($bytes);
                match result {
                    ProcessingResult::Complete { result } => {
                        assert_eq!(result, Some($expected_type));
                    }
                    _ => panic!("Expected Complete(Some(_)), but got {:?}", result),
                }
            }
        };
        ($test_name:ident, $bytes:expr, Complete(None)) => {
            #[test]
            fn $test_name() {
                let result = check_signature($bytes);
                match result {
                    ProcessingResult::Complete { result } => {
                        assert_eq!(result, None);
                    }
                    _ => panic!("Expected Complete(None), but got {:?}", result),
                }
            }
        };
        ($test_name:ident, $bytes:expr, NeedsMoreInput($expected_hint:expr)) => {
            #[test]
            fn $test_name() {
                let result = check_signature($bytes);
                match result {
                    ProcessingResult::NeedsMoreInput { size_hint, .. } => {
                        assert_eq!(size_hint, $expected_hint);
                    }
                    _ => panic!("Expected NeedsMoreInput, but got {:?}", result),
                }
            }
        };
    }

    signature_test!(
        full_container_sig,
        &CONTAINER_SIGNATURE,
        Complete(Some(JxlSignatureType::Container))
    );

    signature_test!(
        partial_container_sig,
        &CONTAINER_SIGNATURE[..5],
        NeedsMoreInput(CONTAINER_SIGNATURE.len() - 5)
    );

    signature_test!(
        full_codestream_sig,
        &CODESTREAM_SIGNATURE,
        Complete(Some(JxlSignatureType::Codestream))
    );

    signature_test!(
        partial_codestream_sig,
        &CODESTREAM_SIGNATURE[..1],
        NeedsMoreInput(CODESTREAM_SIGNATURE.len() - 1)
    );

    signature_test!(
        empty_prefix,
        &[],
        NeedsMoreInput(CODESTREAM_SIGNATURE.len())
    );

    signature_test!(invalid_sig, &[0x12, 0x34, 0x56, 0x77], Complete(None));

    signature_test!(
        container_with_extra_data,
        &{
            let mut data = CONTAINER_SIGNATURE.to_vec();
            data.extend_from_slice(&[0x11, 0x22, 0x33]);
            data
        },
        Complete(Some(JxlSignatureType::Container))
    );

    signature_test!(
        codestream_with_extra_data,
        &{
            let mut data = CODESTREAM_SIGNATURE.to_vec();
            data.extend_from_slice(&[0x44, 0x55, 0x66]);
            data
        },
        Complete(Some(JxlSignatureType::Codestream))
    );
}
