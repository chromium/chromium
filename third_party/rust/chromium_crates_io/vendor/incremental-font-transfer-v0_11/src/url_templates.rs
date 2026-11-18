//! Implementation of the specific variant of URL template expansion required by the IFT specification.
//!
//! Context: <https://w3c.github.io/IFT/Overview.html#url-templates>
//!
//! URL templates in IFT use an series of one byte opcodes to insert literals or expand template variables.
use crate::patchmap::PatchId;
use crate::short_string::{ShortString, ShortStringBuilder};

/// Indicates a malformed URI template was encountered.
#[derive(Debug, PartialEq, Eq)]
pub enum UrlTemplateError {
    InvalidOpCode(u8),
    UnexpectedEndOfBuffer(u8),
    InvalidUtf8,
}

impl std::fmt::Display for UrlTemplateError {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        match self {
            UrlTemplateError::InvalidOpCode(op_code) => write!(f, "Invalid URL template encountered. Unrecognized op code ({})", op_code),
            UrlTemplateError::UnexpectedEndOfBuffer(op_code) => write!(f, "Invalid URL template encountered. Unexpected end of buffer handling literal insertion (op_code = {})", op_code),
            UrlTemplateError::InvalidUtf8 => write!(f, "Invalid utf8 literal encountered in URL template."),
        }
    }
}

enum OpCode {
    InsertLiteral(u8),
    InsertVariable(Variable),
}

enum Variable {
    Id32,
    Digit(u8),
    Id64,
}

impl TryFrom<u8> for OpCode {
    type Error = UrlTemplateError;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        // See: https://w3c.github.io/IFT/Overview.html#url-templates
        if value & (1 << 7) > 0 {
            Ok(OpCode::InsertVariable(value.try_into()?))
        } else if value != 0 {
            Ok(OpCode::InsertLiteral(value))
        } else {
            Err(UrlTemplateError::InvalidOpCode(value))
        }
    }
}

impl TryFrom<u8> for Variable {
    type Error = UrlTemplateError;

    fn try_from(value: u8) -> Result<Self, Self::Error> {
        // See: https://w3c.github.io/IFT/Overview.html#url-templates
        match value {
            128 => Ok(Variable::Id32),
            129 => Ok(Variable::Digit(1)),
            130 => Ok(Variable::Digit(2)),
            131 => Ok(Variable::Digit(3)),
            132 => Ok(Variable::Digit(4)),
            133 => Ok(Variable::Id64),
            _ => Err(UrlTemplateError::InvalidOpCode(value)),
        }
    }
}

struct CachedEncoder<'a> {
    patch_id: &'a PatchId,
    id32: Option<ShortString>,
    id64: Option<ShortString>,
}

impl<'a> CachedEncoder<'a> {
    fn new(patch_id: &'a PatchId) -> Self {
        CachedEncoder {
            patch_id,
            id32: None,
            id64: None,
        }
    }

    fn encode_id32(&mut self) -> &str {
        self.id32
            .get_or_insert_with(|| match self.patch_id {
                PatchId::Numeric(id) => {
                    let id = id.to_be_bytes();
                    let id = &id[count_leading_zeroes(&id)..];
                    BASE32HEX_ENCODER.encode(id)
                }
                PatchId::String(id) => BASE32HEX_ENCODER.encode(id),
            })
            .as_str()
    }

    fn encode_id64(&mut self) -> &str {
        self.id64
            .get_or_insert_with(|| match self.patch_id {
                PatchId::Numeric(id) => {
                    let id = id.to_be_bytes();
                    let id = &id[count_leading_zeroes(&id)..];
                    BASE64URL_PADDED_ENCODER.encode(id)
                }
                PatchId::String(id) => BASE64URL_PADDED_ENCODER.encode(id),
            })
            .as_str()
    }
}

fn id_digit(id_value: &str, digit: u8) -> char {
    id_value
        .len()
        .checked_sub(digit.into())
        .and_then(|index| id_value.as_bytes().get(index).copied())
        .map(|b| b as char)
        .unwrap_or('_')
}

impl std::error::Error for UrlTemplateError {}

/// Implements url template expansion from incremental font transfer.
///
/// Specification: <https://w3c.github.io/IFT/Overview.html#url-templates>
pub(crate) fn expand_template(
    mut template_bytes: &[u8],
    patch_id: &PatchId,
) -> Result<ShortString, UrlTemplateError> {
    let mut cached_encoder = CachedEncoder::new(patch_id);
    let mut output_buffer = ShortStringBuilder::default();
    while let Some((opcode, rest)) = template_bytes.split_first() {
        template_bytes = rest;
        match OpCode::try_from(*opcode)? {
            OpCode::InsertLiteral(count) => {
                let (literals, rest) = template_bytes
                    .split_at_checked(count as usize)
                    .ok_or(UrlTemplateError::UnexpectedEndOfBuffer(*opcode))?;
                template_bytes = rest;
                let s = std::str::from_utf8(literals).map_err(|_| UrlTemplateError::InvalidUtf8)?;
                output_buffer.push_str(s);
            }
            OpCode::InsertVariable(Variable::Id32) => {
                output_buffer.push_str(cached_encoder.encode_id32())
            }
            OpCode::InsertVariable(Variable::Digit(digit)) => {
                output_buffer.push(id_digit(cached_encoder.encode_id32(), digit))
            }
            OpCode::InsertVariable(Variable::Id64) => {
                output_buffer.push_str(cached_encoder.encode_id64())
            }
        }
    }
    let bytes_buffer = output_buffer.build();

    Ok(bytes_buffer)
}

fn count_leading_zeroes(id: &[u8]) -> usize {
    id.iter().take_while(|b| **b == 0).count().min(id.len() - 1)
}

struct Encoder<const N: usize> {
    // The alphabet to encode to.
    alphabet: &'static [u8; N],
    // The alignment of the input. If the input is not a multiple of `input_block_size`, then the
    // space will be filled with `padding_str`.
    input_block_size: usize,
    // The output padding to meet the input padding requirements.
    padding_str: &'static str,
}

const BASE64URL_PADDED_ENCODER: Encoder<64> = Encoder {
    alphabet: b"ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_",
    padding_str: "%3D",
    input_block_size: 3,
};

const BASE32HEX_ENCODER: Encoder<32> = Encoder {
    alphabet: b"0123456789ABCDEFGHIJKLMNOPQRSTUV",
    padding_str: "",
    input_block_size: 1,
};

impl<const N: usize> Encoder<N> {
    const BITS_PER_BYTE: usize = Self::compute_bits_per_byte().unwrap();

    const fn compute_bits_per_byte() -> Option<usize> {
        let bits_per_byte = N.ilog2() as usize;
        if N == (1 << bits_per_byte) {
            Some(bits_per_byte)
        } else {
            None
        }
    }

    fn encode(&self, input: &[u8]) -> ShortString {
        // unwrap ok, validated with compile-time assertions.
        // Create a mask to extract the next N bits.
        // Example, for 6 bits: (1 << 6) - 1 = 64 - 1 = 63 = 0x3F
        let next_bit_mask: u32 = (1 << Self::BITS_PER_BYTE) - 1;
        let input_bits = input.len() * 8;
        let padding_count = if self.input_block_size > 1 {
            let padding_remainder = input.len() % self.input_block_size;
            (self.input_block_size - padding_remainder) % self.input_block_size
        } else {
            0
        };
        let encoded_chars = input_bits.div_ceil(Self::BITS_PER_BYTE);
        let output_len = encoded_chars + (self.padding_str.len() * padding_count);

        let mut output = ShortStringBuilder::with_capacity(output_len);
        let mut bits: u32 = 0;
        let mut bits_in_buffer = 0;
        for byte in input.iter().copied() {
            bits = (bits << 8) | byte as u32;
            bits_in_buffer += 8;
            while bits_in_buffer >= Self::BITS_PER_BYTE {
                bits_in_buffer -= Self::BITS_PER_BYTE;
                let next_byte = bits >> bits_in_buffer;
                let index = next_byte & next_bit_mask;
                output.push(self.alphabet[index as usize] as char);
            }
        }
        if bits_in_buffer > 0 {
            let next_byte = bits << (Self::BITS_PER_BYTE - bits_in_buffer);
            let index = next_byte & next_bit_mask;
            output.push(self.alphabet[index as usize] as char);
        }

        for _ in 0..padding_count {
            output.push_str(self.padding_str);
        }
        output.build()
    }
}

#[cfg(test)]
pub(crate) mod tests {
    use crate::patchmap::PatchId;
    use crate::url_templates::UrlTemplateError;

    use super::{expand_template, BASE32HEX_ENCODER, BASE64URL_PADDED_ENCODER};

    fn check_numeric(bytes: &[u8], id: u32, expected: &str) {
        assert_eq!(
            expand_template(bytes, &PatchId::Numeric(id),),
            Ok(expected.into())
        );
    }

    fn check_string(bytes: &[u8], id: &[u8], expected: &str) {
        assert_eq!(
            expand_template(bytes, &PatchId::String(Vec::from(id)),),
            Ok(expected.into())
        );
    }

    fn check_error(bytes: &[u8], id: u32, expected: UrlTemplateError) {
        assert_eq!(
            expand_template(bytes, &PatchId::Numeric(id),),
            Err(expected)
        );
    }

    #[test]
    fn spec_examples() {
        // From https://w3c.github.io/IFT/Overview.html#url-templates
        check_numeric(
            &[
                16, b'h', b't', b't', b'p', b's', b':', b'/', b'/', b'f', b'o', b'o', b'.', b'b',
                b'a', b'r', b'/', 128,
            ],
            123,
            "https://foo.bar/FC",
        );
        check_numeric(
            &[8, b'f', b'o', b'o', b'?', b'b', b'a', b'r', b'=', 128],
            123,
            "foo?bar=FC",
        );
        check_numeric(
            &[
                10, b'/', b'/', b'f', b'o', b'o', b'.', b'b', b'a', b'r', b'/', 128,
            ],
            0,
            "//foo.bar/00",
        );
        check_numeric(
            &[
                5, b'/', b'f', b'o', b'o', b'/', 129, 1, b'/', 130, 1, b'/', 128,
            ],
            478,
            "/foo/0/F/07F0",
        );
        check_numeric(
            &[
                5, b'/', b'f', b'o', b'o', b'/', 129, 1, b'/', 130, 1, b'/', 131, 1, b'/', 128,
            ],
            123,
            "/foo/C/F/_/FC",
        );

        check_string(
            &[
                4, b'f', b'o', b'o', b'/', 129, 1, b'/', 130, 1, b'/', 131, 1, b'/', 128,
            ],
            b"baz",
            "foo/K/N/G/C9GNK",
        );

        check_string(
            &[
                4, b'f', b'o', b'o', b'/', 129, 1, b'/', 130, 1, b'/', 131, 1, b'/', 128,
            ],
            b"z",
            "foo/8/F/_/F8",
        );

        check_numeric(
            &[
                10, b'/', b'/', b'f', b'o', b'o', b'.', b'b', b'a', b'r', b'/', 133,
            ],
            14_000_000,
            "//foo.bar/1Z-A",
        );
        check_numeric(
            &[
                10, b'/', b'/', b'f', b'o', b'o', b'.', b'b', b'a', b'r', b'/', 133,
            ],
            0,
            "//foo.bar/AA%3D%3D",
        );
        check_numeric(
            &[
                10, b'/', b'/', b'f', b'o', b'o', b'.', b'b', b'a', b'r', b'/', 133,
            ],
            17_000_000,
            "//foo.bar/AQNmQA%3D%3D",
        );
        check_string(
            &[
                10, b'/', b'/', b'f', b'o', b'o', b'.', b'b', b'a', b'r', b'/', 133,
            ],
            &[0xc3, 0xa0, 0x62, 0x63],
            "//foo.bar/w6BiYw%3D%3D",
        );
    }

    #[test]
    fn spec_error_examples() {
        // From https://w3c.github.io/IFT/Overview.html#url-templates
        check_error(
            &[4, b'f', b'o', b'o', b'/', 150],
            123,
            UrlTemplateError::InvalidOpCode(150),
        );

        check_error(
            &[4, b'f', b'o', b'o', b'/', 0, 128],
            123,
            UrlTemplateError::InvalidOpCode(0),
        );

        check_error(
            &[10, b'f', b'o', b'o', b'/', 128],
            123,
            UrlTemplateError::UnexpectedEndOfBuffer(10),
        );

        check_error(
            &[4, b'f', b'o', b'o', 0x85, 128],
            123,
            UrlTemplateError::InvalidUtf8,
        );
    }

    #[test]
    fn empty_template() {
        check_numeric(&[], 123, "");
    }

    #[test]
    fn copied_literals_only() {
        check_numeric(
            &[7, b'f', b'o', b'o', b'0', b'b', b'a', b'r'],
            123,
            "foo0bar",
        );
    }

    #[test]
    fn expansion_only() {
        check_numeric(&[128, 133], 123, "FCew%3D%3D");
    }

    #[test]
    fn base64url_empty_input_returns_empty_string() {
        assert_eq!(BASE64URL_PADDED_ENCODER.encode(&[]), "");
    }

    #[test]
    fn base64url_input_length_multiple_of_3_returns_unpadded_string() {
        assert_eq!(BASE64URL_PADDED_ENCODER.encode(&[1, 2, 3]), "AQID");
    }

    #[test]
    fn base64url_input_length_remainder_returns_padded_string() {
        assert_eq!(BASE64URL_PADDED_ENCODER.encode(&[1, 2]), "AQI%3D");
        assert_eq!(BASE64URL_PADDED_ENCODER.encode(&[1]), "AQ%3D%3D");
    }

    #[test]
    fn base64url_returns_correct_encoding() {
        assert_eq!(BASE64URL_PADDED_ENCODER.encode(&[0, 0, 0]), "AAAA");
        assert_eq!(BASE64URL_PADDED_ENCODER.encode(&[10, 20, 100]), "ChRk");
        assert_eq!(BASE64URL_PADDED_ENCODER.encode(&[255, 255, 255]), "____");
        assert_eq!(
            BASE64URL_PADDED_ENCODER.encode(&[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]),
            "AQIDBAUGBwgJCg%3D%3D"
        );
    }

    #[test]
    fn base32hex_empty_input_returns_empty_string() {
        assert_eq!(BASE32HEX_ENCODER.encode(&[]), "");
    }

    #[test]
    fn base32hex_multi_byte_input_returns_unpadded_string() {
        assert_eq!(BASE32HEX_ENCODER.encode(&[0, 0]), "0000");
        assert_eq!(BASE32HEX_ENCODER.encode(&[255, 255]), "VVVG");
        assert_eq!(BASE32HEX_ENCODER.encode(&[255]), "VS");
        assert_eq!(BASE32HEX_ENCODER.encode(&[1, 2]), "0410");
        assert_eq!(
            BASE32HEX_ENCODER.encode(&[1, 2, 3, 4, 5, 6, 7, 8, 9, 10]),
            "041061050O3GG28A"
        );
    }

    #[test]
    fn base64url_full_alphabet() {
        assert_eq!(
            BASE64URL_PADDED_ENCODER.encode(&[
                0, 16, 131, 16, 81, 135, 32, 146, 139, 48, 211, 143, 65, 20, 147, 81, 85, 151, 97,
                150, 155, 113, 215, 159, 130, 24, 163, 146, 89, 167, 162, 154, 171, 178, 219, 175,
                195, 28, 179, 211, 93, 183, 227, 158, 187, 243, 223, 191
            ]),
            "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789-_"
        );
    }

    #[test]
    fn base32hex_full_alphabet() {
        assert_eq!(
            BASE32HEX_ENCODER.encode(&[
                0, 68, 50, 20, 199, 66, 84, 182, 53, 207, 132, 101, 58, 86, 215, 198, 117, 190,
                119, 223
            ]),
            "0123456789ABCDEFGHIJKLMNOPQRSTUV"
        );
    }
}
