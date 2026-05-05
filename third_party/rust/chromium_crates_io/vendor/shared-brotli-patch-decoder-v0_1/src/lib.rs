#[cfg(feature = "c-brotli")]
mod c_brotli;

#[cfg(feature = "c-brotli")]
mod sys;

#[cfg(feature = "rust-brotli")]
mod rust_brotli;

pub mod decode_error;

#[cfg(fuzzing)]
use decode_error::DecodeError;

/// A Shared Brotli Decoder.
///
/// Shared brotli (<https://datatracker.ietf.org/doc/draft-vandevenne-shared-brotli-format/>) is an
/// extension of brotli to allow the decompression to include a shared
/// dictionary.
pub trait SharedBrotliDecoder {
    /// Decodes shared brotli encoded data using the optional shared dictionary.
    ///
    /// The shared dictionary is a raw LZ77 style dictionary, see:
    /// <https://datatracker.ietf.org/doc/html/draft-vandevenne-shared-brotli-format#section-3.2>
    ///
    /// Will fail if the decoded result will be greater than
    /// max_uncompressed_length. Any excess data in encoded after the
    /// encoded stream finishes is also considered an error.
    fn decode(
        &self,
        encoded: &[u8],
        shared_dictionary: Option<&[u8]>,
        max_uncompressed_length: usize,
    ) -> Result<Vec<u8>, decode_error::DecodeError>;
}

/// The brotli decoder provided by this crate.
///
/// By default a rust wrapper around the c brotli decoder implementation is
/// used.
pub struct BuiltInBrotliDecoder;

/// An implementation that just passes through the input data.
///
/// Useful in fuzzers and unit testing.
pub struct NoopBrotliDecoder;

impl SharedBrotliDecoder for Box<dyn SharedBrotliDecoder> {
    fn decode(
        &self,
        encoded: &[u8],
        shared_dictionary: Option<&[u8]>,
        max_uncompressed_length: usize,
    ) -> Result<Vec<u8>, decode_error::DecodeError> {
        self.as_ref().decode(encoded, shared_dictionary, max_uncompressed_length)
    }
}

impl SharedBrotliDecoder for BuiltInBrotliDecoder {
    fn decode(
        &self,
        encoded: &[u8],
        shared_dictionary: Option<&[u8]>,
        max_uncompressed_length: usize,
    ) -> Result<Vec<u8>, decode_error::DecodeError> {
        cfg_if::cfg_if! {
            if #[cfg(feature = "c-brotli")] {
                #[allow(clippy::needless_return)]
                return c_brotli::shared_brotli_decode_c(
                    encoded,
                    shared_dictionary,
                    max_uncompressed_length,
                );
            } else if #[cfg(feature = "rust-brotli")] {
                return rust_brotli::shared_brotli_decode_rust(encoded, shared_dictionary, max_uncompressed_length);
            } else {
                compile_error!("At least one of 'c-brotli' or 'rust-brotli' must be enabled.");
            }
        }
    }
}

impl SharedBrotliDecoder for NoopBrotliDecoder {
    fn decode(
        &self,
        encoded: &[u8],
        _shared_dictionary: Option<&[u8]>,
        max_uncompressed_length: usize,
    ) -> Result<Vec<u8>, decode_error::DecodeError> {
        if encoded.len() <= max_uncompressed_length {
            Ok(encoded.to_vec())
        } else {
            Err(decode_error::DecodeError::MaxSizeExceeded)
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use decode_error::DecodeError;

    const TARGET: &[u8] = "hijkabcdeflmnohijkabcdeflmno\n".as_bytes();
    const BASE: &str = "abcdef\n";

    // This patch was manually generated with a brotli encoder (https://github.com/google/brotli)
    // uncompressed = TARGET
    // dict = BASE
    const SHARED_DICT_PATCH: [u8; 23] = [
        0xa1, 0xe0, 0x00, 0xc0, 0x2f, 0x3a, 0x38, 0xf4, 0x01, 0xd1, 0xaf, 0x54, 0x84, 0x14, 0x71,
        0x2a, 0x80, 0x04, 0xa2, 0x1c, 0xd3, 0xdd, 0x07,
    ];

    // This patch was manually generated with a brotli encoder (https://github.com/google/brotli)
    // uncompressed = TARGET
    const NO_DICT_PATCH: [u8; 26] = [
        0xa1, 0xe0, 0x00, 0xc0, 0x2f, 0x96, 0x1c, 0xf3, 0x03, 0xb1, 0xcf, 0x45, 0x95, 0x22, 0x4a,
        0xc5, 0x03, 0x21, 0xb2, 0x9a, 0x58, 0xd4, 0x7c, 0xf6, 0x1e, 0x00u8,
    ];

    #[test]
    fn brotli_decode_with_shared_dict() {
        assert_eq!(
            Ok(TARGET.to_vec()),
            BuiltInBrotliDecoder.decode(&SHARED_DICT_PATCH, Some(BASE.as_bytes()), TARGET.len(),)
        );
    }

    #[test]
    fn brotli_decode_rust_brotli_regression_case() {
        // Tests a case that triggered a bug in the rust brotli decompressor library,
        // see: https://github.com/dropbox/rust-brotli-decompressor/issues/36
        let patch: &[u8] = &[
            27, 103, 0, 96, 47, 14, 120, 211, 142, 228, 22, 15, 167, 193, 55, 28, 228, 226, 254,
            54, 10, 36, 226, 192, 19, 76, 50, 8, 169, 92, 9, 197, 47, 12, 211, 114, 34, 175, 18,
            241, 122, 134, 170, 32, 189, 4, 112, 153, 119, 12, 237, 23, 120, 130, 2,
        ];

        let dict: Vec<u8> = vec![
            2, 0, 0, 0, 0, 213, 195, 31, 121, 231, 225, 250, 238, 34, 174, 158, 246, 208, 145, 187,
            92, 2, 0, 0, 4, 0, 0, 0, 46, 0, 0, 0, 0, 0, 11, 123, 105, 100, 125, 46, 105, 102, 116,
            95, 116, 107, 20, 0, 0, 52, 40, 103, 221, 215, 223, 255, 95, 54, 15, 13, 85, 53, 206,
            115, 249, 165, 159, 159, 16, 29, 37, 17, 114, 1, 163, 2, 16, 33, 51, 4, 32, 0, 226, 29,
            19, 88, 254, 195, 129, 23, 25, 22, 8, 19, 21, 41, 130, 136, 51, 8, 67, 209, 52, 204,
            204, 70, 199, 130, 252, 47, 16, 40, 186, 251, 62, 63, 19, 236, 147, 240, 211, 215, 59,
        ];

        let decompressed = BuiltInBrotliDecoder.decode(patch, Some(&dict), 500);

        assert_eq!(
            decompressed,
            Ok(vec![
                0x02, 0x00, 0x00, 0x00, 0x00, 0x8c, 0x16, 0xa6, 0x25, 0x18, 0xf8, 0x68, 0x63, 0x4e,
                0xe4, 0x09, 0x2b, 0xa1, 0xe2, 0x4b, 0xba, 0x02, 0x00, 0x00, 0x03, 0x00, 0x00, 0x00,
                0x2e, 0x00, 0x00, 0x00, 0x00, 0x00, 0x0b, 0x7b, 0x69, 0x64, 0x7d, 0x2e, 0x69, 0x66,
                0x74, 0x5f, 0x74, 0x6b, 0x14, 0x00, 0x00, 0x38, 0x1d, 0x25, 0x11, 0x72, 0x01, 0xa3,
                0x02, 0x10, 0x21, 0x33, 0x04, 0x20, 0x00, 0xe2, 0x1d, 0x13, 0x58, 0xfe, 0xc3, 0x81,
                0x17, 0x19, 0x16, 0x08, 0x13, 0x15, 0x29, 0x82, 0x88, 0x33, 0x08, 0x43, 0xd1, 0x34,
                0xcc, 0xcc, 0x46, 0xc7, 0x82, 0xfc, 0x2f, 0x10, 0x28, 0xba, 0xfb, 0x3e, 0x3f, 0x13,
                0xec, 0x93, 0xf0, 0xd3, 0xd7, 0x3b,
            ])
        );
    }

    #[test]
    fn brotli_decode_without_shared_dict() {
        let base = "".as_bytes();

        assert_eq!(
            Ok(TARGET.to_vec()),
            BuiltInBrotliDecoder.decode(&NO_DICT_PATCH, None, TARGET.len())
        );

        // Check that empty base is handled the same as no base.
        assert_eq!(
            Ok(TARGET.to_vec()),
            BuiltInBrotliDecoder.decode(&NO_DICT_PATCH, Some(base), TARGET.len())
        );
    }

    #[test]
    fn brotli_decode_too_little_output() {
        assert_eq!(
            Err(DecodeError::MaxSizeExceeded),
            BuiltInBrotliDecoder.decode(
                &SHARED_DICT_PATCH,
                Some(BASE.as_bytes()),
                TARGET.len() - 1
            )
        );
    }

    #[test]
    fn brotli_decode_excess_output() {
        assert_eq!(
            Ok(TARGET.to_vec()),
            BuiltInBrotliDecoder.decode(
                &SHARED_DICT_PATCH,
                Some(BASE.as_bytes()),
                TARGET.len() + 1,
            )
        );
    }

    // TODO(garretrieger): there doesn't seem to be an easy way to detect this
    // condition with the rust brotli implementation. So disable for now.
    // However, we need to make this behaviour consistent between the two
    // possible implementations. Either don't check for this in the c
    // version, or figure out how to have a similar check in rust.
    #[cfg(feature = "c-brotli")]
    #[test]
    fn brotli_decode_too_much_input() {
        let mut patch: Vec<u8> = NO_DICT_PATCH.to_vec();
        patch.push(0u8);

        assert_eq!(
            Err(DecodeError::ExcessInputData),
            BuiltInBrotliDecoder.decode(&patch, None, TARGET.len())
        );
    }

    #[test]
    fn brotli_decode_input_missing() {
        // Check what happens if input stream is missing some trailing bytes
        let patch: Vec<u8> = NO_DICT_PATCH[..NO_DICT_PATCH.len() - 1].to_vec();
        assert!(matches!(
            BuiltInBrotliDecoder.decode(&patch, None, TARGET.len()),
            Err(DecodeError::InvalidStream)
        ));
    }

    #[test]
    fn brotli_decode_invalid() {
        let patch = [0xFF, 0xFF, 0xFFu8];
        assert!(matches!(
            BuiltInBrotliDecoder.decode(&patch, None, 10),
            Err(DecodeError::InvalidStream)
        ));
    }
}
