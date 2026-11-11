use base64::engine;

pub(crate) const BASE64: engine::GeneralPurpose = engine::GeneralPurpose::new(
    &base64::alphabet::STANDARD,
    engine::GeneralPurposeConfig::new()
        .with_decode_allow_trailing_bits(true)
        .with_decode_padding_mode(engine::DecodePaddingMode::Indifferent)
        .with_encode_padding(true),
);

pub(crate) const BASE64_CANONICAL: engine::GeneralPurpose = engine::GeneralPurpose::new(
    &base64::alphabet::STANDARD,
    engine::GeneralPurposeConfig::new()
        .with_decode_allow_trailing_bits(true)
        .with_decode_padding_mode(engine::DecodePaddingMode::RequireCanonical)
        .with_encode_padding(true),
);

pub(crate) const BASE64_NO_PADDING: engine::GeneralPurpose = engine::GeneralPurpose::new(
    &base64::alphabet::STANDARD,
    engine::GeneralPurposeConfig::new()
        .with_decode_allow_trailing_bits(true)
        .with_decode_padding_mode(engine::DecodePaddingMode::RequireNone)
        .with_encode_padding(true),
);

const fn is_tchar(c: u8) -> bool {
    // See tchar values list in https://tools.ietf.org/html/rfc7230#section-3.2.6
    matches!(
        c,
        b'!' | b'#'
            | b'$'
            | b'%'
            | b'&'
            | b'\''
            | b'*'
            | b'+'
            | b'-'
            | b'.'
            | b'^'
            | b'_'
            | b'`'
            | b'|'
            | b'~'
    ) || c.is_ascii_alphanumeric()
}

pub(crate) const fn is_allowed_start_token_char(c: u8) -> bool {
    c.is_ascii_alphabetic() || c == b'*'
}

pub(crate) const fn is_allowed_inner_token_char(c: u8) -> bool {
    is_tchar(c) || c == b':' || c == b'/'
}

pub(crate) const fn is_allowed_start_key_char(c: u8) -> bool {
    c.is_ascii_lowercase() || c == b'*'
}

pub(crate) const fn is_allowed_inner_key_char(c: u8) -> bool {
    c.is_ascii_lowercase() || c.is_ascii_digit() || matches!(c, b'_' | b'-' | b'*' | b'.')
}

#[cfg(feature = "arbitrary")]
macro_rules! impl_arbitrary {
    ($($ref:ty, $owned:ty, $min_size:expr;)+) => {
        $(
            impl<'a> arbitrary::Arbitrary<'a> for &'a $ref {
                fn arbitrary(u: &mut arbitrary::Unstructured<'a>) -> arbitrary::Result<Self> {
                    let bytes = <&[u8]>::arbitrary(u)?;
                    match <$ref>::from_bytes(bytes) {
                        Ok(res) => Ok(res),
                        Err(err) => {
                            let i = err.index().ok_or(arbitrary::Error::NotEnoughData)?;
                            <$ref>::from_bytes(&bytes[..i])
                                .map_err(|_| arbitrary::Error::IncorrectFormat)
                        }
                    }
                }

                fn size_hint(_depth: usize) -> (usize, Option<usize>) {
                    ($min_size, None)
                }
            }

            impl<'a> arbitrary::Arbitrary<'a> for $owned {
                fn arbitrary(u: &mut arbitrary::Unstructured<'a>) -> arbitrary::Result<Self> {
                    <&$ref>::arbitrary(u).map(ToOwned::to_owned)
                }

                fn size_hint(_depth: usize) -> (usize, Option<usize>) {
                    ($min_size, None)
                }
            }
        )+
    };
}

#[cfg(feature = "arbitrary")]
impl_arbitrary! {
    crate::KeyRef, crate::Key, 1;
    crate::StringRef, crate::String, 0;
    crate::TokenRef, crate::Token, 1;
}
