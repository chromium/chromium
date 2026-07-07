//! Font format detection.

use crate::{FileRef, FontRead};

/// Format for a blob of font data.
#[derive(Copy, Clone, PartialEq, Eq, Hash, Debug)]
pub enum FontFormat {
    /// An [SFNT](https://en.wikipedia.org/wiki/SFNT)-based font, which includes
    /// TrueType and OpenType fonts.
    ///
    /// This is the most common format for fonts.
    ///
    /// The field contains the number of available fonts. This is always 1 for
    /// .ttf and .otf files and usually greater than 1 for .ttc and .otc files.
    Sfnt(u32),
    /// A Type1 font.
    Type1,
    /// A pure CFF font.
    ///
    /// The field contains the number of available fonts.
    Cff(u32),
}

impl FontFormat {
    /// Returns the format of the font data in the given buffer.
    pub fn new(data: &[u8]) -> Option<Self> {
        if let Ok(file) = FileRef::new(data) {
            let format = match file {
                FileRef::Collection(collection) => Self::Sfnt(collection.len()),
                FileRef::Font(_) => Self::Sfnt(1),
            };
            Some(format)
        } else if check_type1(data) {
            Some(Self::Type1)
        } else if let Ok(cff) = crate::ps::cff::v1::Cff::read(data.into()) {
            Some(Self::Cff(cff.top_dicts().count() as u32))
        } else {
            None
        }
    }

    /// Returns true if this is an SFNT-based font.
    pub fn is_sfnt(&self) -> bool {
        matches!(self, Self::Sfnt(_))
    }

    /// Returns the number of available fonts.
    pub fn num_fonts(&self) -> u32 {
        match self {
            Self::Sfnt(n) | Self::Cff(n) => *n,
            _ => 1,
        }
    }
}

fn check_type1(data: &[u8]) -> bool {
    fn check(data: &[u8]) -> bool {
        data.starts_with(b"%!PS-AdobeFont") || data.starts_with(b"%!FontType")
    }
    check(data) || data.get(6..).map(check).unwrap_or(false)
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{FontRef, TableProvider};

    #[test]
    fn check_formats() {
        let pure_cff = FontRef::new(font_test_data::MATERIAL_ICONS_SUBSET)
            .unwrap()
            .cff()
            .unwrap()
            .offset_data()
            .as_bytes();
        use FontFormat::*;
        #[rustfmt::skip]
        let pairs = [
            (font_test_data::CANTARELL_VF_TRIMMED, Sfnt(1)),
            (font_test_data::TINOS_SUBSET, Sfnt(1)),
            (pure_cff, Cff(1)),
            (font_test_data::ttc::TTC, Sfnt(2)),
            (font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFA, Type1),
        ];
        for (data, expected_format) in pairs {
            assert_eq!(FontFormat::new(data).unwrap(), expected_format);
        }
        assert!(FontFormat::new(b"I'm not a font").is_none());
    }
}
