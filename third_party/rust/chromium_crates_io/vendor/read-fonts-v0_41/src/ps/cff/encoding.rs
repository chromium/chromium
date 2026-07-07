//! PostScript encodings.
//!
//! This maps font specific character codes to string ids.
//!
//! See "Glyph Organization" at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=18>
//! for an explanation of how charsets, encodings and glyphs are related.

use super::charset::Charset;
use crate::{
    ps::{encoding::PredefinedEncoding, string::Sid},
    FontData, GlyphId, ReadError,
};

#[doc(inline)]
pub use super::v1::{EncodingRange1 as Range1, EncodingSupplement as Supplement};

/// Mapping from character codes to string ids.
///
/// See "Encodings" at <https://adobe-type-tools.github.io/font-tech-notes/pdfs/5176.CFF.pdf#page=18>.
#[derive(Clone)]
pub enum Encoding<'a> {
    Predefined(PredefinedEncoding),
    Custom(CustomEncoding<'a>),
}

impl<'a> Encoding<'a> {
    /// Parses an encoding at the given offset.
    ///
    /// Special offsets 0 and 1 are parsed as the predefined standard and
    /// expert encodings, respectively.
    pub fn new(data: &'a [u8], offset: usize) -> Result<Self, ReadError> {
        match offset {
            0 => Ok(Self::Predefined(PredefinedEncoding::Standard)),
            1 => Ok(Self::Predefined(PredefinedEncoding::Expert)),
            _ => CustomEncoding::new(data.get(offset..).ok_or(ReadError::OutOfBounds)?)
                .map(Self::Custom),
        }
    }

    /// Maps a character code to a glyph identifier.
    pub fn map(&self, charset: &Charset, code: u8) -> Option<GlyphId> {
        match self {
            Self::Predefined(predefined) => charset.glyph_id(predefined.sid(code)?).ok(),
            Self::Custom(custom) => custom.map(charset, code),
        }
    }
}

/// Custom mapping from character codes to string ids.
#[derive(Clone)]
pub enum CustomEncoding<'a> {
    /// Sequence of character codes where the string id is equal to the index
    /// of the code plus one.
    Format0(&'a [u8], &'a [Supplement]),
    /// Sequence of ranges mapping character codes to string ids.
    Format1(&'a [Range1], &'a [Supplement]),
}

impl<'a> CustomEncoding<'a> {
    /// Parses a custom encoding from the given data.
    pub fn new(data: &'a [u8]) -> Result<Self, ReadError> {
        let mut cursor = FontData::new(data).cursor();
        let header = cursor.read::<u8>()?;
        let has_supplement = header & 0x80 != 0;
        // Macro because a closure cannot borrow cursor mutably
        macro_rules! read_supplement {
            () => {
                if has_supplement {
                    let count = cursor.read::<u8>()?;
                    cursor.read_array::<Supplement>(count as usize)?
                } else {
                    &[]
                }
            };
        }
        let format = header & 0x7F;
        match format {
            0 => {
                let n_codes = cursor.read::<u8>()?;
                let codes = cursor.read_array(n_codes as usize)?;
                let supp = read_supplement!();
                Ok(Self::Format0(codes, supp))
            }
            1 => {
                let n_ranges = cursor.read::<u8>()?;
                let ranges = cursor.read_array(n_ranges as usize)?;
                let supp = read_supplement!();
                Ok(Self::Format1(ranges, supp))
            }
            _ => Err(ReadError::InvalidFormat(format as _)),
        }
    }

    /// Maps a character code to a glyph identifier.  
    pub fn map(&self, charset: &Charset, code: u8) -> Option<GlyphId> {
        let read_sup = |sup: &[Supplement]| {
            sup.iter()
                .find(|s| s.code == code)
                .and_then(|s| charset.glyph_id(Sid::new(s.glyph.get())).ok())
        };
        match self {
            Self::Format0(codes, sup) => read_sup(sup).or_else(|| {
                codes
                    .iter()
                    .position(|c| *c == code)
                    // notdef is implicit so add one
                    .map(|gid| GlyphId::new(gid as u32 + 1))
            }),
            Self::Format1(ranges, sup) => read_sup(sup).or_else(|| {
                let mut gid = 1u32;
                for range in ranges.iter() {
                    let end = range.first.saturating_add(range.n_left);
                    if (range.first..=end).contains(&code) {
                        gid += (code - range.first) as u32;
                        return Some(GlyphId::new(gid));
                    }
                    gid += range.n_left as u32 + 1;
                }
                None
            }),
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;

    #[test]
    fn predefined_standard() {
        let encoding = Encoding::Predefined(PredefinedEncoding::Standard);
        let charset = iso_adobe_charset();
        for code in 0..=255 {
            let gid = encoding.map(&charset, code);
            assert_eq!(
                gid.unwrap(),
                charset
                    .glyph_id(PredefinedEncoding::Standard.sid(code).unwrap())
                    .unwrap()
            );
        }
    }

    #[test]
    fn predefined_expert() {
        let encoding = Encoding::Predefined(PredefinedEncoding::Expert);
        let charset = iso_expert_charset();
        for code in 0..=255 {
            let gid = encoding.map(&charset, code);
            assert_eq!(
                gid.unwrap(),
                charset
                    .glyph_id(PredefinedEncoding::Expert.sid(code).unwrap())
                    .unwrap()
            );
        }
    }

    #[test]
    fn custom_format_0() {
        let codes = [3, 8, 9, 10, 11];
        let encoding = Encoding::Custom(CustomEncoding::Format0(&codes, &[]));
        let charset = iso_adobe_charset();
        for (i, code) in codes.into_iter().enumerate() {
            assert_eq!(
                encoding.map(&charset, code).unwrap(),
                GlyphId::new(i as u32 + 1)
            );
        }
    }

    #[test]
    fn custom_format_1() {
        let ranges = [(51, 4), (250, 5)].map(|(first, n_left)| Range1 { first, n_left });
        let encoding = Encoding::Custom(CustomEncoding::Format1(&ranges, &[]));
        let charset = iso_adobe_charset();
        for code in 0..=255 {
            let gid = encoding.map(&charset, code);
            let expected = match code {
                51..=55 => Some(code as u32 - 50),
                250..=255 => Some(code as u32 - 250 + 6),
                _ => None,
            };
            assert_eq!(gid, expected.map(GlyphId::new));
        }
    }

    #[test]
    fn supplemental() {
        // map 40 -> z and 122 -> parenleft
        let supplement = [(40, 91), (122, 9)].map(|(code, glyph)| Supplement {
            code,
            glyph: glyph.into(),
        });
        let encoding = Encoding::Custom(CustomEncoding::Format0(&[], &supplement));
        let charset = iso_adobe_charset();
        assert_eq!(encoding.map(&charset, 40).unwrap().to_u32(), 91);
        assert_eq!(encoding.map(&charset, 122).unwrap().to_u32(), 9);
        assert_eq!(
            charset
                .string_id(91u32.into())
                .unwrap()
                .resolve_standard()
                .unwrap(),
            b"z"
        );
        assert_eq!(
            charset
                .string_id(9u32.into())
                .unwrap()
                .resolve_standard()
                .unwrap(),
            b"parenleft"
        );
    }

    fn iso_adobe_charset() -> Charset<'static> {
        Charset::new(Default::default(), 0, 256).unwrap()
    }

    fn iso_expert_charset() -> Charset<'static> {
        Charset::new(Default::default(), 1, 256).unwrap()
    }
}
