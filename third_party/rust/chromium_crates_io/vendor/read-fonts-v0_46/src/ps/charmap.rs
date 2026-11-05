//! Unicode character map generated from glyph names.

#[cfg(feature = "agl")]
use super::agl;
use alloc::vec::Vec;
use types::GlyphId;

/// Used to mark variant glyphs such as A.alt.
const VARIANT_BIT: u32 = 0x80000000;

/// A Unicode charmap built from glyph names.
///
/// Building one depends on the `agl` feature.
#[derive(Clone, Default, Debug)]
pub struct UnicodeCharmap {
    mapping: Vec<(u32, GlyphId)>,
}

impl UnicodeCharmap {
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
            let ch = unicode_value(name);
            if ch & !VARIANT_BIT == 0 {
                continue;
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
        // Base glyphs sort before their variants. The sort is stable, so
        // glyphs with the same value stay in glyph order, as the C library
        // sort FreeType uses keeps them on most systems.
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psnames/psmodule.c#L182>
        mapping.sort_by_key(|(ch, _)| (ch & !VARIANT_BIT, *ch));
        Self { mapping }
    }

    /// Returns the glyph id for the given character.
    ///
    /// A base glyph is preferred, and a variant is used where the font has
    /// no base glyph for the character.
    pub fn map(&self, ch: impl Into<u32>) -> Option<GlyphId> {
        // <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psnames/psmodule.c#L412>
        self.search(ch.into(), false).0
    }

    /// Returns the first character above `ch` that has a glyph, and the
    /// glyph.
    ///
    /// <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psnames/psmodule.c#L456>
    pub(crate) fn next_char(&self, ch: u32) -> Option<(u32, GlyphId)> {
        let ch = ch.checked_add(1)?;
        match self.search(ch, true) {
            (Some(glyph), _) => Some((ch, glyph)),
            // The first entry above, which is a base glyph where there is
            // one, since bases sort first.
            (None, min) => self
                .mapping
                .get(min)
                .map(|(next, glyph)| (next & !VARIANT_BIT, *glyph)),
        }
    }

    /// Searches for `ch` as FreeType does, returning the glyph found and the
    /// index of the first entry at or above `ch`.
    ///
    /// The search keeps a variant until it finds the base glyph, and after
    /// each step predicts the next entry as in a run of consecutive
    /// characters. Where several glyphs share a character, the predictions
    /// decide which is found. A step below the current entry is a large
    /// unsigned step forwards. `char_next` keeps a 32 bit index, so the step
    /// wraps around to the entry below. `char_index` keeps a pointer, which
    /// wraps the same way on 32 bit targets and lands past the end on 64 bit
    /// ones, so its results depend on the target.
    fn search(&self, ch: u32, wrapping_index: bool) -> (Option<GlyphId>, usize) {
        let entries = &self.mapping;
        let (mut min, mut max) = (0, entries.len());
        let mut mid = min + ((max - min) >> 1);
        let mut result = None;
        while min < max {
            let Some(&(value, glyph)) = entries.get(mid) else {
                break;
            };
            if value == ch {
                return (Some(glyph), min);
            }
            let base = value & !VARIANT_BIT;
            if base == ch {
                // Remember the variant but keep looking for a base glyph.
                result = Some(glyph);
            }
            if base < ch {
                min = mid + 1;
            } else {
                max = mid;
            }
            // A reasonable prediction in a run of consecutive characters.
            let step = ch.wrapping_sub(base);
            mid = if wrapping_index || cfg!(target_pointer_width = "32") {
                (mid as u32).wrapping_add(step) as usize
            } else {
                mid.saturating_add(step as usize)
            };
            if mid >= max || mid < min {
                mid = min + ((max - min) >> 1);
            }
        }
        (result, min)
    }

    /// Returns each character and its glyph, in character order.
    ///
    /// A variant glyph, such as `A.alt`, has the high bit set on its
    /// character, and follows any base glyph for the character.
    pub fn iter(&self) -> impl Iterator<Item = (u32, GlyphId)> + Clone + '_ {
        self.mapping.iter().copied()
    }
}

/// Returns the code point FreeType assigns `name`, with [`VARIANT_BIT`] set
/// for a name with a suffix after a dot, or zero if it has none.
///
/// A `uni` name takes exactly four uppercase hex digits and a `u` name four
/// to six. Neither is checked for a valid code point, so either can map a
/// surrogate or a value past U+10FFFF. Any other name is looked up whole up
/// to its first dot, so ligatures such as `f_i` and longer `uni` sequences
/// map to nothing.
///
/// See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/80a507a6b8e3d2906ad2c8ba69329bd2fb2a85ef/src/psnames/psmodule.c#L61>
#[cfg(feature = "agl")]
fn unicode_value(name: &str) -> u32 {
    /// Reads up to `max` uppercase hex digits, returning their value and
    /// count.
    fn hex_digits(bytes: &[u8], max: usize) -> (u32, usize) {
        let mut value = 0;
        let mut count = 0;
        for &b in bytes.iter().take(max) {
            let digit = match b {
                b'0'..=b'9' => b - b'0',
                b'A'..=b'F' => b - b'A' + 10,
                _ => break,
            };
            value = (value << 4) | digit as u32;
            count += 1;
        }
        (value, count)
    }
    /// Returns `value` if the digits end the name or a dot follows them.
    fn ended(value: u32, rest: &[u8]) -> Option<u32> {
        match rest.first() {
            None => Some(value),
            Some(b'.') => Some(value | VARIANT_BIT),
            _ => None,
        }
    }
    let bytes = name.as_bytes();
    if let Some(digits) = bytes.strip_prefix(b"uni") {
        let (value, count) = hex_digits(digits, 4);
        if let Some(value) = (count == 4)
            .then(|| ended(value, &digits[count..]))
            .flatten()
        {
            return value;
        }
    }
    if let Some(digits) = bytes.strip_prefix(b"u") {
        let (value, count) = hex_digits(digits, 6);
        if let Some(value) = (count >= 4)
            .then(|| ended(value, &digits[count..]))
            .flatten()
        {
            return value;
        }
    }
    let base = name.split('.').next().unwrap_or_default();
    if base.is_empty() {
        return 0;
    }
    let value = agl::lookup_char(base).map_or(0, |ch| ch as u32);
    if base.len() < name.len() {
        value | VARIANT_BIT
    } else {
        value
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
        let cmap = UnicodeCharmap::from_glyph_names(
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
        let cmap = UnicodeCharmap::from_glyph_names(font.glyph_names());
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
        let cmap = UnicodeCharmap::from_glyph_names([(GlyphId::new(1), "hyphen")].into_iter());
        assert_eq!(cmap.mapping.len(), 2);
        assert_eq!(cmap.map('\u{00AD}'), Some(GlyphId::new(1)));
        assert_eq!(cmap.map('\u{002D}'), Some(GlyphId::new(1)));
    }

    #[test]
    fn extra_glyphs_no_override() {
        // If we have an explicit "softhyphen" glyph then don't add any
        // additional mapping
        let cmap = UnicodeCharmap::from_glyph_names([(GlyphId::new(1), "softhyphen")].into_iter());
        assert_eq!(cmap.mapping.len(), 1);
        assert_eq!(cmap.map('\u{00AD}'), Some(GlyphId::new(1)));
    }

    // The values FreeType's psnames module gives these names.
    #[test]
    fn names_convert_as_freetype_converts_them() {
        const V: u32 = VARIANT_BIT;
        for (name, value) in [
            ("uni0041", 0x41),
            ("uni0041.alt", 0x41 | V),
            ("u0041.alt.sc", 0x41 | V),
            ("A.", 0x41 | V),
            ("A..B", 0x41 | V),
            ("union", 0x222A),
            ("a1", 0x2701),
            ("a191.alt", 0x27BE | V),
            // Out of range values are kept.
            ("uniD800", 0xD800),
            ("u110000", 0x110000),
            ("uFFFFFF", 0xFFFFFF),
            ("u1F600", 0x1F600),
            // These map to nothing.
            ("uni0000", 0),
            ("u0000", 0),
            ("u0000041", 0),
            ("uni00410042", 0),
            ("uni0041x", 0),
            ("f_i", 0),
            ("dalethatafpatah", 0),
            ("eigujarati", 0),
            (".notdef", 0),
        ] {
            assert_eq!(unicode_value(name), value, "{name}");
        }
    }

    /// Steps through `cmap` as FreeType does, stopping at glyph zero.
    fn walk(cmap: &UnicodeCharmap) -> Vec<(u32, u32)> {
        let mut entries = Vec::new();
        let mut code = 0;
        while let Some((next, glyph)) = cmap.next_char(code) {
            if glyph == GlyphId::NOTDEF {
                break;
            }
            entries.push((next, glyph.to_u32()));
            code = next;
        }
        entries
    }

    // The glyphs FreeType's psnames module finds and steps through for these
    // glyph names, where several glyphs share a character.
    #[test]
    fn search_and_walk_match_freetype() {
        const V: u32 = VARIANT_BIT;
        type Case<'a> = (&'a str, &'a [(u32, u32)], &'a [(u32, u32)]);
        let cases: [Case; 9] = [
            // The search lands on the second glyph and the walk on the first.
            (".notdef A A", &[(0x41, 2)], &[(0x41, 1)]),
            (".notdef D D D D D", &[(0x44, 3)], &[(0x44, 1)]),
            (".notdef C.alt C.swash C.alt", &[(0x43, 1)], &[(0x43, 1)]),
            (
                ".notdef A.alt A.sc A",
                &[(0x41, 3), (0x41 | V, 1)],
                &[(0x41, 3)],
            ),
            (
                ".notdef A.alt A.alt A.alt A.alt B",
                &[(0x41, 1), (0x41 | V, 3), (0x42, 5)],
                &[(0x41, 1), (0x42, 5)],
            ),
            // Glyph zero ends the walk.
            ("space A B", &[(0x41, 1), (0x42, 2)], &[]),
            (
                ".notdef uniD800 u110000 uni0000 A",
                &[(0x41, 4), (0xD800, 1), (0x110000, 2), (0, 0)],
                &[(0x41, 4), (0xD800, 1), (0x110000, 2)],
            ),
            (
                ".notdef hyphen x a1 spade",
                &[(0x2D, 1), (0xAD, 1), (0x2701, 3), (0x2660, 4)],
                &[(0x2D, 1), (0x78, 2), (0xAD, 1), (0x2660, 4), (0x2701, 3)],
            ),
            // Searching for B and G steps below the current entry, which
            // FreeType's search takes as the entry below on 32 bit targets
            // and as past the end on 64 bit ones. Its walk wraps on both.
            (
                ".notdef A uni0041 B uni0042 C uni0043 G uni0047 H",
                if cfg!(target_pointer_width = "32") {
                    &[
                        (0x41, 2),
                        (0x42, 4),
                        (0x43, 5),
                        (0x44, 0),
                        (0x47, 8),
                        (0x48, 9),
                    ]
                } else {
                    &[
                        (0x41, 2),
                        (0x42, 3),
                        (0x43, 5),
                        (0x44, 0),
                        (0x47, 7),
                        (0x48, 9),
                    ]
                },
                &[(0x41, 1), (0x42, 4), (0x43, 5), (0x47, 7), (0x48, 9)],
            ),
        ];
        for (names, mapped, walked) in cases {
            let cmap = UnicodeCharmap::from_glyph_names(
                names
                    .split(' ')
                    .enumerate()
                    .map(|(glyph, name)| (GlyphId::new(glyph as u32), name)),
            );
            for &(ch, glyph) in mapped {
                assert_eq!(
                    cmap.map(ch).map_or(0, |glyph| glyph.to_u32()),
                    glyph,
                    "{names}: {ch:x}"
                );
            }
            assert_eq!(walk(&cmap), walked, "{names}");
        }
    }
}
