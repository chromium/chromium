//! Mapping of characters (codepoints, not graphemes) to nominal glyph identifiers.
//!
//! If you have never run into character to glyph mapping before
//! [Glyph IDs and the 'cmap' table](https://rsheeter.github.io/font101/#glyph-ids-and-the-cmap-table)
//! might be informative.
//!
//! The functionality in this module provides a 1-to-1 mapping from Unicode
//! characters (or [Unicode variation sequences](http://unicode.org/faq/vs.html)) to
//! nominal or "default" internal glyph identifiers for a given font.
//! This is a necessary first step, but generally insufficient for proper layout of
//! [complex text](https://en.wikipedia.org/wiki/Complex_text_layout) or even
//! simple text containing diacritics and ligatures.
//!
//! Comprehensive mapping of characters to positioned glyphs requires a process called
//! shaping. For more detail, see: [Why do I need a shaping engine?](https://harfbuzz.github.io/why-do-i-need-a-shaping-engine.html)

use read_fonts::{
    tables::cmap::{
        self, Cmap, Cmap12, Cmap12Iter, Cmap13, Cmap13Iter, Cmap4, Cmap4Iter, CmapIterLimits,
        CmapSubtable, EncodingRecord, PlatformId, UvsSubtable, UvsSubtableIter,
    },
    types::GlyphId,
    FontData, FontRef, TableProvider,
};

pub use read_fonts::tables::cmap::MapVariant;

/// Mapping of characters to nominal glyph identifiers.
///
/// The mappings are derived from the [cmap](https://learn.microsoft.com/en-us/typography/opentype/spec/cmap)
/// table. Depending on the font, the returned mapping may have entries that point to virtual/phantom glyph
/// ids beyond the glyph count of the `MAXP` or `maxp` table, which are only used during the shaping process,
/// for example.
/// An optional `DMAP` table (ISO/IEC 14496-22:2026) is consulted first, with
/// unmapped characters and variation sequences falling back to `cmap`.
///
/// ## Obtaining a Charmap
///
/// Typically a Charmap is acquired by calling [charmap](crate::MetadataProvider::charmap) on a [FontRef].
///
/// ## Selection strategy
///
/// Fonts may contain multiple subtables in various formats supporting different encodings. The selection
/// strategy implemented here is designed to choose mappings that capture the broadest available Unicode
/// coverage:
///
/// * Unicode characters: a symbol mapping subtable is selected if available. Otherwise, subtables supporting
///   the Unicode full repertoire or Basic Multilingual Plane (BMP) are preferred, in that order. Formats
///   [4](https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-4-segment-mapping-to-delta-values)
///   and [12](https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-12-segmented-coverage) are
///   supported.
///
/// * Unicode variation sequences: these are provided by a format
///   [14](https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-14-unicode-variation-sequences)
///   or format 15 (ISO/IEC 14496-22:2026) subtable, preferring format 15.
///
#[derive(Clone, Default)]
pub struct Charmap<'a> {
    cmap: TableCharmap<'a>,
    dmap: TableCharmap<'a>,
    cmap_limits: CmapIterLimits,
}

#[derive(Clone, Default)]
struct TableCharmap<'a> {
    codepoint_subtable: Option<CodepointSubtable<'a>>,
    variant_subtable: Option<UvsSubtable<'a>>,
}

impl<'a> Charmap<'a> {
    /// Creates a new character map from the given font.
    pub fn new(font: &FontRef<'a>) -> Self {
        Self {
            cmap: TableCharmap::new(font.cmap().ok()),
            dmap: TableCharmap::new(font.dmap().ok().map(|table| table.as_cmap())),
            cmap_limits: CmapIterLimits::default_for_font(font),
        }
    }

    /// Returns true if a suitable Unicode character mapping is available.
    pub fn has_map(&self) -> bool {
        self.dmap.codepoint_subtable.is_some() || self.cmap.codepoint_subtable.is_some()
    }

    /// Returns true if a symbol mapping was selected.
    pub fn is_symbol(&self) -> bool {
        [&self.dmap, &self.cmap].iter().any(|table| {
            table
                .codepoint_subtable
                .as_ref()
                .is_some_and(|x| x.is_symbol)
        })
    }

    /// Returns true if a Unicode variation sequence mapping is available.
    pub fn has_variant_map(&self) -> bool {
        self.dmap.variant_subtable.is_some() || self.cmap.variant_subtable.is_some()
    }

    /// Maps a character to a nominal glyph identifier.
    ///
    /// Returns `None` if a mapping does not exist.
    pub fn map(&self, ch: impl Into<u32>) -> Option<GlyphId> {
        let ch = ch.into();
        self.dmap
            .codepoint_subtable
            .as_ref()
            .and_then(|table| table.map(ch))
            .or_else(|| self.cmap.codepoint_subtable.as_ref()?.map(ch))
    }

    /// Returns an iterator over all mappings of codepoint to nominal glyph
    /// identifiers in the character map.
    pub fn mappings(&self) -> Mappings<'a> {
        Mappings {
            dmap: self.dmap.mappings(self.cmap_limits).peekable(),
            cmap: self.cmap.mappings(self.cmap_limits).peekable(),
        }
    }

    /// Maps a character and variation selector to a nominal glyph identifier.
    ///
    /// Returns `None` if a mapping does not exist.
    pub fn map_variant(&self, ch: impl Into<u32>, selector: impl Into<u32>) -> Option<MapVariant> {
        let (ch, selector) = (ch.into(), selector.into());
        self.dmap
            .variant_subtable
            .as_ref()
            .and_then(|table| table.map_variant(ch, selector))
            .or_else(|| {
                self.cmap
                    .variant_subtable
                    .as_ref()?
                    .map_variant(ch, selector)
            })
    }

    /// Returns an iterator over all mappings of character and variation
    /// selector to nominal glyph identifier in the character map.
    pub fn variant_mappings(&self) -> VariantMappings<'a> {
        VariantMappings {
            dmap: self
                .dmap
                .variant_subtable
                .clone()
                .map(|table| table.iter_with_limits(self.cmap_limits)),
            cmap: self
                .cmap
                .variant_subtable
                .clone()
                .map(|table| table.iter_with_limits(self.cmap_limits)),
            dmap_subtable: self.dmap.variant_subtable.clone(),
        }
    }
}

impl<'a> TableCharmap<'a> {
    fn new(cmap: Option<Cmap<'a>>) -> Self {
        let Some(cmap) = cmap else {
            return Self::default();
        };
        let selection = MappingSelection::new(&cmap);
        Self {
            codepoint_subtable: selection
                .codepoint_subtable
                .map(|subtable| CodepointSubtable {
                    subtable,
                    is_symbol: selection.mapping_index.codepoint_subtable_is_symbol,
                }),
            variant_subtable: selection.variant_subtable,
        }
    }

    fn mappings(&self, limits: CmapIterLimits) -> SubtableMappings<'a> {
        self.codepoint_subtable
            .as_ref()
            .map(|subtable| {
                SubtableMappings(match &subtable.subtable {
                    SupportedSubtable::Format4(cmap4) => MappingsInner::Format4(cmap4.iter()),
                    SupportedSubtable::Format12(cmap12) => {
                        MappingsInner::Format12(cmap12.iter_with_limits(limits))
                    }
                    SupportedSubtable::Format13(cmap13) => {
                        MappingsInner::Format13(cmap13.iter_with_limits(limits))
                    }
                })
            })
            .unwrap_or(SubtableMappings(MappingsInner::None))
    }
}

/// Cacheable indices of selected mapping tables for materializing a character
/// map.
///
/// Since [`Charmap`] carries a lifetime, it is difficult to store in a cache.
/// This type serves as an acceleration structure that allows for construction
/// of a character map while skipping the search for the most suitable Unicode
/// mappings.
#[derive(Copy, Clone, Default, Debug)]
pub struct MappingIndex {
    cmap: TableMappingIndex,
    dmap: TableMappingIndex,
    cmap_limits: CmapIterLimits,
}

#[derive(Copy, Clone, Default, Debug)]
struct TableMappingIndex {
    /// Index of Unicode or symbol mapping subtable.
    codepoint_subtable: Option<u16>,
    /// True if the above is a symbol mapping.
    codepoint_subtable_is_symbol: bool,
    /// Index of Unicode variation selector subtable.
    variant_subtable: Option<u16>,
}

impl MappingIndex {
    /// Finds the indices of the most suitable Unicode mapping tables in the
    /// given font.
    pub fn new(font: &FontRef) -> Self {
        let index = |table: Option<Cmap<'_>>| {
            table
                .map(|table| MappingSelection::new(&table).mapping_index)
                .unwrap_or_default()
        };
        Self {
            cmap: index(font.cmap().ok()),
            dmap: index(font.dmap().ok().map(|table| table.as_cmap())),
            cmap_limits: CmapIterLimits::default_for_font(font),
        }
    }

    /// Creates a new character map for the given font using the tables referenced by
    /// the precomputed indices.
    ///
    /// The font should be the same as the one used to construct this object.
    pub fn charmap<'a>(&self, font: &FontRef<'a>) -> Charmap<'a> {
        Charmap {
            cmap: self.cmap.charmap(font.cmap().ok()),
            dmap: self
                .dmap
                .charmap(font.dmap().ok().map(|table| table.as_cmap())),
            cmap_limits: self.cmap_limits,
        }
    }
}

impl TableMappingIndex {
    fn charmap<'a>(&self, cmap: Option<Cmap<'a>>) -> TableCharmap<'a> {
        let Some(cmap) = cmap else {
            return TableCharmap::default();
        };
        let records = cmap.encoding_records();
        let data = cmap.offset_data();
        TableCharmap {
            codepoint_subtable: self
                .codepoint_subtable
                .and_then(|index| get_subtable(data, records, index))
                .and_then(SupportedSubtable::new)
                .map(|subtable| CodepointSubtable {
                    subtable,
                    is_symbol: self.codepoint_subtable_is_symbol,
                }),
            variant_subtable: self
                .variant_subtable
                .and_then(|index| get_subtable(data, records, index))
                .and_then(|subtable| match subtable {
                    CmapSubtable::Format14(table) => Some(UvsSubtable::from(table)),
                    CmapSubtable::Format15(table) => Some(UvsSubtable::from(table)),
                    _ => None,
                }),
        }
    }
}

/// Iterator over all mappings of character to nominal glyph identifier
/// in a character map.
///
/// This is created with the [`Charmap::mappings`] method.
#[derive(Clone)]
pub struct Mappings<'a> {
    dmap: core::iter::Peekable<SubtableMappings<'a>>,
    cmap: core::iter::Peekable<SubtableMappings<'a>>,
}

impl Iterator for Mappings<'_> {
    type Item = (u32, GlyphId);

    fn next(&mut self) -> Option<Self::Item> {
        match (self.dmap.peek(), self.cmap.peek()) {
            (Some(delta), Some(base)) if delta.0 == base.0 => {
                self.cmap.next();
                self.dmap.next()
            }
            (Some(delta), Some(base)) if delta.0 < base.0 => self.dmap.next(),
            (Some(_), None) => self.dmap.next(),
            _ => self.cmap.next(),
        }
    }
}

#[derive(Clone)]
struct SubtableMappings<'a>(MappingsInner<'a>);

impl Iterator for SubtableMappings<'_> {
    type Item = (u32, GlyphId);

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let item = match &mut self.0 {
                MappingsInner::None => None,
                MappingsInner::Format4(iter) => iter.next(),
                MappingsInner::Format12(iter) => iter.next(),
                MappingsInner::Format13(iter) => iter.next(),
            }?;
            if item.1 != GlyphId::NOTDEF {
                return Some(item);
            }
        }
    }
}

#[derive(Clone)]
enum MappingsInner<'a> {
    None,
    Format4(Cmap4Iter<'a>),
    Format12(Cmap12Iter<'a>),
    Format13(Cmap13Iter<'a>),
}

/// Iterator over all mappings of character and variation selector to
/// nominal glyph identifier in a character map.
///
/// This is created with the [`Charmap::variant_mappings`] method.
#[derive(Clone)]
pub struct VariantMappings<'a> {
    dmap: Option<UvsSubtableIter<'a>>,
    cmap: Option<UvsSubtableIter<'a>>,
    dmap_subtable: Option<UvsSubtable<'a>>,
}

impl Iterator for VariantMappings<'_> {
    type Item = (u32, u32, MapVariant);

    fn next(&mut self) -> Option<Self::Item> {
        if let Some(item) = self.dmap.as_mut().and_then(Iterator::next) {
            return Some(item);
        }
        self.cmap.as_mut()?.find(|(ch, selector, _)| {
            self.dmap_subtable
                .as_ref()
                .and_then(|table| table.map_variant(*ch, *selector))
                .is_none()
        })
    }
}

fn get_subtable<'a>(
    data: FontData<'a>,
    records: &[EncodingRecord],
    index: u16,
) -> Option<CmapSubtable<'a>> {
    records
        .get(index as usize)
        .and_then(|record| record.subtable(data).ok())
}

#[derive(Clone)]
struct CodepointSubtable<'a> {
    subtable: SupportedSubtable<'a>,
    /// True if the subtable is a symbol mapping.
    is_symbol: bool,
}

impl CodepointSubtable<'_> {
    fn map(&self, codepoint: u32) -> Option<GlyphId> {
        self.map_impl(codepoint).or_else(|| {
            if self.is_symbol && codepoint <= 0x00FF {
                // From HarfBuzz:
                // For symbol-encoded OpenType fonts, we duplicate the
                // U+F000..F0FF range at U+0000..U+00FF.  That's what
                // Windows seems to do, and that's hinted about at:
                // https://docs.microsoft.com/en-us/typography/opentype/spec/recom
                // under "Non-Standard (Symbol) Fonts".
                // See <https://github.com/harfbuzz/harfbuzz/blob/453ded05392af38bba9f89587edce465e86ffa6b/src/hb-ot-cmap-table.hh#L1595>
                self.map_impl(codepoint + 0xF000)
            } else {
                None
            }
        })
    }

    fn map_impl(&self, codepoint: u32) -> Option<GlyphId> {
        let gid = match &self.subtable {
            SupportedSubtable::Format4(subtable) => subtable.map_codepoint(codepoint),
            SupportedSubtable::Format12(subtable) => subtable.map_codepoint(codepoint),
            SupportedSubtable::Format13(subtable) => subtable.map_codepoint(codepoint),
        }?;
        (gid != GlyphId::NOTDEF).then_some(gid)
    }
}

#[derive(Clone)]
enum SupportedSubtable<'a> {
    Format4(Cmap4<'a>),
    Format12(Cmap12<'a>),
    Format13(Cmap13<'a>),
}

impl<'a> SupportedSubtable<'a> {
    fn new(subtable: CmapSubtable<'a>) -> Option<Self> {
        Some(match subtable {
            CmapSubtable::Format4(cmap4) => Self::Format4(cmap4),
            CmapSubtable::Format12(cmap12) => Self::Format12(cmap12),
            CmapSubtable::Format13(cmap13) => Self::Format13(cmap13),
            _ => return None,
        })
    }

    fn from_cmap_record(cmap: &Cmap<'a>, record: &cmap::EncodingRecord) -> Option<Self> {
        Self::new(record.subtable(cmap.offset_data()).ok()?)
    }
}

/// The mapping kind of a cmap subtable.
///
/// The ordering is significant and determines the priority of subtable
/// selection (greater is better).
#[derive(Copy, Clone, PartialEq, PartialOrd)]
enum MappingKind {
    None = 0,
    UnicodeBmp = 1,
    UnicodeFull = 2,
    Symbol = 3,
}

/// The result of searching the cmap table for the "best" available
/// subtables.
///
/// For `codepoint_subtable`, best means either symbol (which is preferred)
/// or a Unicode subtable with the greatest coverage.
///
/// For `variant_subtable`, best means format 15, falling back to format 14.
struct MappingSelection<'a> {
    /// The mapping index accelerator that holds indices of the following
    /// subtables.
    mapping_index: TableMappingIndex,
    /// Either a symbol subtable or the Unicode subtable with the
    /// greatest coverage.
    codepoint_subtable: Option<SupportedSubtable<'a>>,
    /// Subtable that supports mapping Unicode variation sequences.
    variant_subtable: Option<UvsSubtable<'a>>,
}

impl<'a> MappingSelection<'a> {
    fn new(cmap: &Cmap<'a>) -> Self {
        const ENCODING_MS_SYMBOL: u16 = 0;
        const ENCODING_MS_UNICODE_CS: u16 = 1;
        const ENCODING_APPLE_ID_UNICODE_32: u16 = 4;
        const ENCODING_APPLE_ID_VARIANT_SELECTOR: u16 = 5;
        const ENCODING_UNICODE_FULL_REPERTOIRE: u16 = 6;
        const ENCODING_MS_ID_UCS_4: u16 = 10;
        let mut mapping_index = TableMappingIndex::default();
        let mut mapping_kind = MappingKind::None;
        let mut codepoint_subtable = None;
        let mut variant_subtable = None;
        if let Some((index, subtable)) = cmap.uvs_subtable() {
            mapping_index.variant_subtable = Some(index);
            variant_subtable = Some(subtable);
        }
        let mut maybe_choose_subtable = |kind, index, subtable| {
            if kind > mapping_kind {
                mapping_kind = kind;
                mapping_index.codepoint_subtable_is_symbol = kind == MappingKind::Symbol;
                mapping_index.codepoint_subtable = Some(index as u16);
                codepoint_subtable = Some(subtable);
            }
        };
        // This generally follows the same strategy as FreeType, searching the encoding
        // records in reverse and prioritizing UCS-4 subtables over UCS-2.
        // See <https://gitlab.freedesktop.org/freetype/freetype/-/blob/ac5babe87629107c43f627e2cd17c6cf4f2ecd43/src/base/ftobjs.c#L1370>
        // The exception is that we prefer a symbol subtable over all others which matches the behavior
        // of HarfBuzz.
        // See <https://github.com/harfbuzz/harfbuzz/blob/453ded05392af38bba9f89587edce465e86ffa6b/src/hb-ot-cmap-table.hh#L1818>
        for (i, record) in cmap.encoding_records().iter().enumerate().rev() {
            match (record.platform_id(), record.encoding_id()) {
                // Unicode variation sequences are chosen by Cmap::uvs_subtable.
                (PlatformId::Unicode, ENCODING_APPLE_ID_VARIANT_SELECTOR) => {}
                (PlatformId::Windows, ENCODING_MS_SYMBOL) => {
                    // Symbol
                    if let Some(subtable) = SupportedSubtable::from_cmap_record(cmap, record) {
                        maybe_choose_subtable(MappingKind::Symbol, i, subtable);
                    }
                }
                (PlatformId::Windows, ENCODING_MS_ID_UCS_4)
                | (PlatformId::Unicode, ENCODING_APPLE_ID_UNICODE_32)
                | (PlatformId::Unicode, ENCODING_UNICODE_FULL_REPERTOIRE) => {
                    // Unicode full repertoire
                    if let Some(subtable) = SupportedSubtable::from_cmap_record(cmap, record) {
                        maybe_choose_subtable(MappingKind::UnicodeFull, i, subtable);
                    }
                }
                (PlatformId::ISO, _)
                | (PlatformId::Unicode, _)
                | (PlatformId::Windows, ENCODING_MS_UNICODE_CS) => {
                    // Unicode BMP only
                    if let Some(subtable) = SupportedSubtable::from_cmap_record(cmap, record) {
                        maybe_choose_subtable(MappingKind::UnicodeBmp, i, subtable);
                    }
                }
                _ => {}
            }
        }
        Self {
            mapping_index,
            codepoint_subtable,
            variant_subtable,
        }
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::MetadataProvider;
    use read_fonts::FontRef;

    #[test]
    fn format15_variants_and_cached_selection() {
        use font_test_data::cmap::{font_with_cmaps, format12, format14, format15, table};
        let nominal = format12(&[(65, 1)]);
        let old = format14(0xfe0f, &[], &[(65, 2), (67, 3)]);
        let new = format15(0xfe0f, &[65], &[(66, 70000), (68, 0xffffff)]);
        for records in [
            vec![
                (3, 10, nominal.as_slice()),
                (0, 5, old.as_slice()),
                (0, 5, new.as_slice()),
            ],
            vec![
                (3, 10, nominal.as_slice()),
                (0, 5, new.as_slice()),
                (0, 5, old.as_slice()),
            ],
        ] {
            let base = table(&records);
            let data = font_with_cmaps(Some(&base), None);
            let font = FontRef::new(&data).unwrap();
            for charmap in [font.charmap(), MappingIndex::new(&font).charmap(&font)] {
                assert!(charmap.has_variant_map());
                let expected = [
                    (65, 0xfe0f, MapVariant::UseDefault),
                    (66, 0xfe0f, MapVariant::Variant(GlyphId::new(70000))),
                    (68, 0xfe0f, MapVariant::Variant(GlyphId::new(0xffffff))),
                ];
                assert_eq!(charmap.variant_mappings().collect::<Vec<_>>(), expected);
                for (ch, selector, mapping) in expected {
                    assert_eq!(charmap.map_variant(ch, selector), Some(mapping));
                }
                assert_eq!(charmap.map_variant(67u32, 0xfe0fu32), None);
            }
        }
    }

    #[test]
    fn dmap_variation_precedence_across_formats() {
        use font_test_data::cmap::{font_with_cmaps, format14, format15, table};
        let old = format14(0xfe0f, &[], &[(65, 1), (66, 2)]);
        let new = format15(0xfe0f, &[], &[(65, 70000), (67, 80000)]);
        for (base, delta, expected) in [
            (&old, &new, vec![(65u32, 70000), (66, 2), (67, 80000)]),
            (&new, &old, vec![(65u32, 1), (66, 2), (67, 80000)]),
        ] {
            let base = table(&[(0, 5, base)]);
            let delta = table(&[(0, 5, delta)]);
            for cmap in [Some(base.as_slice()), None] {
                let data = font_with_cmaps(cmap, Some(&delta));
                let font = FontRef::new(&data).unwrap();
                for charmap in [font.charmap(), MappingIndex::new(&font).charmap(&font)] {
                    let mut seen = Vec::new();
                    for (ch, selector, mapping) in charmap.variant_mappings() {
                        assert_eq!(charmap.map_variant(ch, selector), Some(mapping));
                        assert!(!seen.contains(&(ch, selector)));
                        seen.push((ch, selector));
                    }
                    if cmap.is_some() {
                        assert_eq!(seen.len(), expected.len());
                        for &(ch, glyph) in &expected {
                            assert_eq!(
                                charmap.map_variant(ch, 0xfe0fu32),
                                Some(MapVariant::Variant(GlyphId::new(glyph)))
                            );
                        }
                    } else {
                        assert_eq!(seen.len(), 2);
                    }
                }
            }
        }
    }

    #[test]
    fn dmap_overrides_and_falls_back_to_cmap() {
        use font_test_data::cmap::{font_with_cmaps, format12, table};
        let base = table(&[(3, 10, &format12(&[(65, 1), (66, 2), (68, 5)]))]);
        let delta = table(&[(0, 4, &format12(&[(65, 3), (66, 0), (67, 4)]))]);
        let data = font_with_cmaps(Some(&base), Some(&delta));
        let font = FontRef::new(&data).unwrap();
        for charmap in [font.charmap(), MappingIndex::new(&font).charmap(&font)] {
            assert!(charmap.has_map());
            for (ch, gid) in [('A', 3), ('B', 2), ('C', 4), ('D', 5)] {
                assert_eq!(charmap.map(ch), Some(GlyphId::new(gid)));
            }
            assert_eq!(charmap.map('E'), None);
            assert_eq!(
                charmap.mappings().collect::<Vec<_>>(),
                vec![
                    (65, GlyphId::new(3)),
                    (66, GlyphId::new(2)),
                    (67, GlyphId::new(4)),
                    (68, GlyphId::new(5))
                ]
            );
        }
    }

    #[test]
    fn dmap_only_and_malformed_dmap() {
        use font_test_data::cmap::{font_with_cmaps, format12, table};
        let delta = table(&[(3, 10, &format12(&[(65, 70000)]))]);
        let data = font_with_cmaps(None, Some(&delta));
        let font = FontRef::new(&data).unwrap();
        assert_eq!(
            font.dmap().unwrap().as_cmap().map_codepoint('A'),
            Some(GlyphId::new(70000))
        );
        for charmap in [font.charmap(), MappingIndex::new(&font).charmap(&font)] {
            assert!(charmap.has_map());
            assert_eq!(charmap.map('A'), Some(GlyphId::new(70000)));
        }
        let data = font_with_cmaps(Some(&delta), Some(&[0, 0]));
        let font = FontRef::new(&data).unwrap();
        for charmap in [font.charmap(), MappingIndex::new(&font).charmap(&font)] {
            assert_eq!(charmap.map('A'), Some(GlyphId::new(70000)));
        }
    }

    #[test]
    fn dmap_variants_override_and_fall_back_to_cmap() {
        use font_test_data::cmap::{font_with_cmaps, format12, format14, table};
        let base = table(&[
            (3, 10, &format12(&[(65, 1), (66, 2)])),
            (0, 5, &format14(0xfe0f, &[66], &[(65, 5), (67, 6)])),
        ]);
        let delta = table(&[
            (3, 10, &format12(&[(65, 3)])),
            (0, 5, &format14(0xfe0f, &[65], &[(66, 4), (68, 7)])),
        ]);
        let data = font_with_cmaps(Some(&base), Some(&delta));
        let font = FontRef::new(&data).unwrap();
        for charmap in [font.charmap(), MappingIndex::new(&font).charmap(&font)] {
            assert!(charmap.has_variant_map());
            assert_eq!(charmap.map('A'), Some(GlyphId::new(3)));
            let expected = [
                (65, MapVariant::UseDefault),
                (66, MapVariant::Variant(GlyphId::new(4))),
                (67, MapVariant::Variant(GlyphId::new(6))),
                (68, MapVariant::Variant(GlyphId::new(7))),
            ];
            let variants: Vec<_> = charmap.variant_mappings().collect();
            assert_eq!(variants.len(), expected.len());
            for (ch, variant) in expected {
                assert_eq!(charmap.map_variant(ch, 0xfe0fu32), Some(variant));
                assert!(variants.contains(&(ch, 0xfe0f, variant)));
            }
            assert_eq!(charmap.map_variant(69u32, 0xfe0fu32), None);
        }
        let data = font_with_cmaps(None, Some(&delta));
        let font = FontRef::new(&data).unwrap();
        assert_eq!(
            font.charmap().map_variant('B', 0xfe0fu32),
            Some(MapVariant::Variant(GlyphId::new(4)))
        );
    }

    #[test]
    fn choose_format_12_over_4() {
        let font = FontRef::new(font_test_data::CMAP12_FONT1).unwrap();
        let charmap = font.charmap();
        assert!(matches!(
            charmap.cmap.codepoint_subtable.unwrap().subtable,
            SupportedSubtable::Format12(..)
        ));
    }

    #[test]
    fn dmap_and_cmap15_agree_across_public_charmaps_at_boundaries() {
        use font_test_data::cmap::{font_with_cmaps, format12, format15, table};
        use read_fonts::model::Font;

        let selector = 0xe0100;
        let base = table(&[
            (
                3,
                10,
                &format12(&[(0x20000, 1), (0x20001, 2), (0x20002, 2), (0x10ffff, 4)]),
            ),
            (
                0,
                5,
                &format15(
                    selector,
                    &[],
                    &[
                        (0x20000, 3),
                        (0x20001, 4),
                        (0x20002, 65536),
                        (0x10ffff, 65535),
                    ],
                ),
            ),
        ]);
        let delta = table(&[
            (
                3,
                10,
                &format12(&[(0x20000, 3), (0x20001, 4), (0x20002, 0), (0x10ffff, 5)]),
            ),
            (
                0,
                5,
                &format15(selector, &[0x20000], &[(0x20001, 0xffffff)]),
            ),
        ]);
        let bytes = font_with_cmaps(Some(&base), Some(&delta));
        let font = FontRef::new(&bytes).unwrap();
        let cached = Font::new(bytes.clone(), 0).unwrap();
        let index = MappingIndex::new(&font);
        let nominal = [(0x20000, 3), (0x20001, 4), (0x20002, 2), (0x10ffff, 5)]
            .map(|(ch, gid)| (ch, GlyphId::new(gid)));
        let variants = [
            (0x20000, 3),
            (0x20001, 0xffffff),
            (0x20002, 65536),
            (0x10ffff, 65535),
        ]
        .map(|(ch, gid)| (ch, selector, GlyphId::new(gid)));
        for charmap in [font.charmap(), index.charmap(&font)] {
            let mut mappings: Vec<_> = charmap.mappings().collect();
            mappings.sort_unstable();
            assert_eq!(mappings, nominal);
            let mut resolved: Vec<_> = charmap
                .variant_mappings()
                .map(|(ch, vs, variant)| {
                    let glyph = match variant {
                        MapVariant::UseDefault => charmap.map(ch).unwrap(),
                        MapVariant::Variant(glyph) => glyph,
                    };
                    (ch, vs, glyph)
                })
                .collect();
            resolved.sort_unstable();
            assert_eq!(resolved, variants);
            for (ch, glyph) in nominal {
                assert_eq!(charmap.map(ch), Some(glyph));
            }
            for (ch, vs, glyph) in variants {
                let variant = charmap.map_variant(ch, vs).unwrap();
                assert_eq!(
                    match variant {
                        MapVariant::UseDefault => charmap.map(ch).unwrap(),
                        MapVariant::Variant(glyph) => glyph,
                    },
                    glyph
                );
            }
            assert_eq!(charmap.map_variant(0x20000u32, selector + 1), None);
            assert_eq!(charmap.map(0x20003u32), None);
        }
        for instance in [
            &cached,
            &cached.default_instance(),
            &cached.instance_builder().build(),
        ] {
            let charmap = instance.charmap();
            let mut mappings: Vec<_> = charmap.unicodes().collect();
            mappings.sort_unstable();
            assert_eq!(mappings, nominal);
            let mut resolved: Vec<_> = charmap.unicode_variants().collect();
            resolved.sort_unstable();
            assert_eq!(resolved, variants);
            for (ch, glyph) in nominal {
                assert_eq!(charmap.map_unicode(ch), Some(glyph));
            }
            for (ch, vs, glyph) in variants {
                assert_eq!(charmap.map_unicode_variant(ch, vs), Some(glyph));
            }
        }
    }

    #[test]
    fn choose_format_13_over_4() {
        let font = FontRef::new(font_test_data::TOFU).unwrap();
        let charmap = font.charmap();
        assert!(matches!(
            charmap.cmap.codepoint_subtable.unwrap().subtable,
            SupportedSubtable::Format13(..)
        ));
    }

    #[test]
    fn choose_format_13_over_4_with_unicode_encoding_6() {
        let font = FontRef::new(font_test_data::LAST_RESORT_HE).unwrap();
        let charmap = font.charmap();

        assert!(matches!(
            charmap.cmap.codepoint_subtable.as_ref().unwrap().subtable,
            SupportedSubtable::Format13(..)
        ));
        for ch in ['A', 'あ', '😀'] {
            assert!(charmap.map(ch).is_some(), "no mapping for {ch:?}");
        }
    }

    #[test]
    fn choose_format_4() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let charmap = font.charmap();
        assert!(matches!(
            charmap.cmap.codepoint_subtable.unwrap().subtable,
            SupportedSubtable::Format4(..)
        ));
    }

    #[test]
    fn choose_symbol() {
        let font = FontRef::new(font_test_data::CMAP4_SYMBOL_PUA).unwrap();
        let charmap = font.charmap();
        assert!(charmap.is_symbol());
        assert!(matches!(
            charmap.cmap.codepoint_subtable.unwrap().subtable,
            SupportedSubtable::Format4(..)
        ));
    }

    #[test]
    fn map_format_4() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let charmap = font.charmap();
        assert_eq!(charmap.map('A'), Some(GlyphId::new(1)));
        assert_eq!(charmap.map('À'), Some(GlyphId::new(2)));
        assert_eq!(charmap.map('`'), Some(GlyphId::new(3)));
        assert_eq!(charmap.map('B'), None);
    }

    #[test]
    fn map_format_12() {
        let font = FontRef::new(font_test_data::CMAP12_FONT1).unwrap();
        let charmap = font.charmap();
        assert_eq!(charmap.map(' '), None);
        assert_eq!(charmap.map(0x101723_u32), Some(GlyphId::new(1)));
        assert_eq!(charmap.map(0x101725_u32), Some(GlyphId::new(3)));
        assert_eq!(charmap.map(0x102523_u32), Some(GlyphId::new(6)));
        assert_eq!(charmap.map(0x102526_u32), Some(GlyphId::new(9)));
        assert_eq!(charmap.map(0x102527_u32), Some(GlyphId::new(10)));
    }

    #[test]
    fn map_format_13() {
        let font = FontRef::new(font_test_data::TOFU).unwrap();
        let charmap = font.charmap();
        // This font maps U+0002..U+007E to gid 1
        for ch in 2..0x7F_u32 {
            assert_eq!(charmap.map(ch), Some(GlyphId::new(1)));
        }
    }

    #[test]
    fn map_symbol_pua() {
        let font = FontRef::new(font_test_data::CMAP4_SYMBOL_PUA).unwrap();
        let charmap = font.charmap();
        assert!(charmap.cmap.codepoint_subtable.as_ref().unwrap().is_symbol);
        assert_eq!(charmap.map(0xF001_u32), Some(GlyphId::new(1)));
        assert_eq!(charmap.map(0xF002_u32), Some(GlyphId::new(2)));
        assert_eq!(charmap.map(0xF003_u32), Some(GlyphId::new(3)));
        assert_eq!(charmap.map(0xF0FE_u32), Some(GlyphId::new(4)));
        // The following don't exist in the cmap table and are remapped into the U+F000..F0FF range
        // due to the selection of a symbol mapping subtable.
        assert_eq!(charmap.map(0x1_u32), Some(GlyphId::new(1)));
        assert_eq!(charmap.map(0x2_u32), Some(GlyphId::new(2)));
        assert_eq!(charmap.map(0x3_u32), Some(GlyphId::new(3)));
        assert_eq!(charmap.map(0xFE_u32), Some(GlyphId::new(4)));
    }

    #[test]
    fn map_variants() {
        use super::MapVariant::*;
        let font = FontRef::new(font_test_data::CMAP14_FONT1).unwrap();
        let charmap = font.charmap();
        let selector = '\u{e0100}';
        assert_eq!(charmap.map_variant('a', selector), None);
        assert_eq!(charmap.map_variant('\u{4e00}', selector), Some(UseDefault));
        assert_eq!(charmap.map_variant('\u{4e06}', selector), Some(UseDefault));
        assert_eq!(
            charmap.map_variant('\u{4e08}', selector),
            Some(Variant(GlyphId::new(25)))
        );
        assert_eq!(
            charmap.map_variant('\u{4e09}', selector),
            Some(Variant(GlyphId::new(26)))
        );
    }

    #[test]
    fn mappings() {
        for font_data in [
            font_test_data::VAZIRMATN_VAR,
            font_test_data::CMAP12_FONT1,
            font_test_data::SIMPLE_GLYF,
            font_test_data::CMAP4_SYMBOL_PUA,
            font_test_data::TOFU,
        ] {
            let font = FontRef::new(font_data).unwrap();
            let charmap = font.charmap();
            for (codepoint, glyph_id) in charmap.mappings() {
                assert_ne!(
                    glyph_id,
                    GlyphId::NOTDEF,
                    "we should never encounter notdef glyphs"
                );
                assert_eq!(charmap.map(codepoint), Some(glyph_id));
            }
        }
    }

    #[test]
    fn variant_mappings() {
        let font = FontRef::new(font_test_data::CMAP14_FONT1).unwrap();
        let charmap = font.charmap();
        for (codepoint, selector, variant) in charmap.variant_mappings() {
            assert_eq!(charmap.map_variant(codepoint, selector), Some(variant));
        }
    }
}
