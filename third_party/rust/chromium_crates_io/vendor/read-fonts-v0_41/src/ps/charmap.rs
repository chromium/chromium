//! Unicode character map generated from glyph names.
//!
//! Building a character map depends on the `agl` feature.

#[cfg(feature = "agl")]
use super::agl;
use alloc::vec::Vec;
use types::GlyphId;

/// Used to mark variant glyphs such as A.alt.
const VARIANT_BIT: u32 = 0x80000000;

/// A Unicode charmap built from glyph names.
#[derive(Clone, Default, Debug)]
pub struct Charmap {
    mapping: Vec<(u32, GlyphId)>,
}

impl Charmap {
    /// Create a new unicode charmap for the given sequence of glyph id
    /// and name pairs.
    // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psnames/psmodule.c#L313>
    #[cfg(feature = "agl")]
    pub fn from_glyph_names<'a>(pairs: impl Iterator<Item = (GlyphId, &'a str)>) -> Self {
        #[derive(Copy, Clone, PartialEq)]
        enum State {
            Unchecked,
            Include,
            Exclude,
        }
        let mut extra_glyphs = [(State::Unchecked, GlyphId::NOTDEF); 10];
        let mut mapping = Vec::new();
        for (gid, name) in pairs {
            // Check extra glyphs by name
            if let Some(n) = EXTRA_GLYPH_LIST
                .iter()
                .position(|(_, extra_name)| name == *extra_name)
            {
                let extra = &mut extra_glyphs[n];
                if extra.0 == State::Unchecked {
                    extra.0 = State::Include;
                    extra.1 = gid;
                }
            }
            // Map to a char
            let Some(mut ch) = agl::name_to_char(name).map(|ch| ch as u32) else {
                continue;
            };
            if agl::split_variant(name).1.is_some() {
                // FreeType sets the high bit for variant glyphs
                ch |= VARIANT_BIT;
            }
            // If we have a direct char mapping for an entry in the extra
            // glyph list then disable it
            if let Some(n) = EXTRA_GLYPH_LIST
                .iter()
                .position(|(extra_ch, _)| ch == *extra_ch)
            {
                extra_glyphs[n].0 = State::Exclude;
            }
            mapping.push((ch, gid));
        }
        for ((extra_ch, _), (state, gid)) in EXTRA_GLYPH_LIST.iter().zip(extra_glyphs) {
            if state == State::Include {
                mapping.push((*extra_ch, gid));
            }
        }
        mapping.shrink_to_fit();
        mapping.sort_unstable_by(|a, b| {
            // Custom comparison to properly sort base glyphs and variants
            // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psnames/psmodule.c#L182>
            let a_base = a.0 & !VARIANT_BIT;
            let b_base = b.0 & !VARIANT_BIT;
            if a_base == b_base {
                a.0.cmp(&b.0)
            } else {
                a_base.cmp(&b_base)
            }
        });
        Self { mapping }
    }

    /// Returns the glyph id for the given character.
    pub fn map(&self, ch: impl Into<u32>) -> Option<GlyphId> {
        // Custom binary search that falls back to variants if a base
        // glyph isn't found
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psnames/psmodule.c#L412>
        let ch = ch.into();
        let mut min = 0;
        let mut max = self.mapping.len();
        let mut result = None;
        while min < max {
            let mid = min + ((max - min) >> 1);
            let entry = self.mapping.get(mid)?;
            if entry.0 == ch {
                result = Some(entry.1);
                break;
            }
            let base_gid = entry.0 & !VARIANT_BIT;
            if base_gid == ch {
                // Remember the variant but keep on search for a base
                result = Some(entry.1);
            }
            if base_gid < ch {
                min = mid + 1;
            } else {
                max = mid;
            }
        }
        result
    }

    pub fn iter(&self) -> Iter<'_> {
        Iter(self.mapping.iter().copied())
    }
}

/// Iterator for a character map.
#[derive(Clone)]
pub struct Iter<'a>(core::iter::Copied<core::slice::Iter<'a, (u32, GlyphId)>>);

impl Iterator for Iter<'_> {
    type Item = (u32, GlyphId);

    fn next(&mut self) -> Option<Self::Item> {
        self.0.next()
    }
}

/// Support for extra glyphs not handled well in AGL
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psnames/psmodule.c#L218>
#[cfg(feature = "agl")]
#[rustfmt::skip]
const EXTRA_GLYPH_LIST: [(u32, &str); 10] = [
    // WGL 4
    (0x0394, "Delta"),
    (0x03A9, "Omega"),
    (0x2215, "fraction"),
    (0x00AD, "hyphen"),
    (0x02C9, "macron"),
    (0x03BC, "mu"),
    (0x2219, "periodcentered"),
    (0x00A0, "space"),
    // Romanian
    (0x021A, "Tcommaaccent"),
    (0x021B, "tcommaaccent"),
];

#[cfg(test)]
#[cfg(feature = "agl")]
mod tests {
    use super::super::type1::Type1Font;
    use super::*;

    #[test]
    fn cmap() {
        let cmap = Charmap::from_glyph_names(
            [
                (1, "A"),
                (2, "uni0042"), // B
                (333, "C.alt"),
                (4, "D"),
                (51, "Lcedilla"),
                (22, "Cdot"),
                (8, "aacute"),
                (7, "union"),
            ]
            .map(|(gid, name)| (GlyphId::new(gid), name))
            .iter()
            .copied(),
        );
        assert_eq!(cmap.map('A'), Some(GlyphId::new(1)));
        assert_eq!(cmap.map('B'), Some(GlyphId::new(2)));
        // We're actually missing a glyph for "C" but have "C.alt" which
        // we select by design (matching FT)
        assert_eq!(cmap.map('C'), Some(GlyphId::new(333)));
        assert_eq!(cmap.map('D'), Some(GlyphId::new(4)));
        assert_eq!(cmap.map('Ļ'), Some(GlyphId::new(51)));
        assert_eq!(cmap.map('Ċ'), Some(GlyphId::new(22)));
        assert_eq!(cmap.map('á'), Some(GlyphId::new(8)));
        assert_eq!(cmap.map('∪'), Some(GlyphId::new(7)));
    }

    #[test]
    fn cmap_from_type1() {
        let font = Type1Font::new(font_test_data::type1::NOTO_SERIF_REGULAR_SUBSET_PFB).unwrap();
        let cmap = Charmap::from_glyph_names(font.glyph_names());
        // Extracted from FreeType's generated unicode cmap
        let expected = [
            ('H' as u32, 1),
            // H.c2sc which gets encoded as a variant
            ('H' as u32 | VARIANT_BIT, 8),
            ('f' as u32, 2),
            ('i' as u32, 3),
            ('x' as u32, 4),
        ];
        let result = cmap
            .iter()
            .map(|(ch, gid)| (ch, gid.to_u32()))
            .collect::<Vec<_>>();
        assert_eq!(result, expected);
        assert_eq!(cmap.map('a'), None);
        for (ch, gid) in expected {
            if ch & VARIANT_BIT != 0 {
                assert_eq!(cmap.map(ch), None);
            } else {
                assert_eq!(
                    cmap.map(ch),
                    Some(GlyphId::new(gid)),
                    "cmap failed for {ch} -> {gid}"
                );
            }
        }
    }

    #[test]
    fn extra_glyphs_override() {
        // If we have a "hyphen" glyph but no "softhyphen" then add an
        // additional entry mapping the soft-hyphen codepoint to the
        // hyphen glyph
        let cmap = Charmap::from_glyph_names([(GlyphId::new(1), "hyphen")].into_iter());
        assert_eq!(cmap.mapping.len(), 2);
        assert_eq!(cmap.map('\u{00AD}'), Some(GlyphId::new(1)));
        assert_eq!(cmap.map('\u{002D}'), Some(GlyphId::new(1)));
    }

    #[test]
    fn extra_glyphs_no_override() {
        // If we have an explicit "softhyphen" glyph then don't add any
        // additional mapping
        let cmap = Charmap::from_glyph_names([(GlyphId::new(1), "softhyphen")].into_iter());
        assert_eq!(cmap.mapping.len(), 1);
        assert_eq!(cmap.map('\u{00AD}'), Some(GlyphId::new(1)));
    }
}
