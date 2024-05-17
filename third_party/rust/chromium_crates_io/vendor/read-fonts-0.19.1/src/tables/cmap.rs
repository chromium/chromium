//! The [cmap](https://docs.microsoft.com/en-us/typography/opentype/spec/cmap) table

include!("../../generated/generated_cmap.rs");

/// Result of mapping a codepoint with a variation selector.
#[derive(Copy, Clone, PartialEq, Eq, Debug)]
pub enum MapVariant {
    /// The variation selector should be ignored and the default mapping
    /// of the character should be used.
    UseDefault,
    /// The variant glyph mapped by a codepoint and associated variation
    /// selector.
    Variant(GlyphId),
}

impl<'a> Cmap<'a> {
    /// Map a codepoint to a nominal glyph identifier
    ///
    /// This uses the first available subtable that provides a valid mapping.
    ///
    /// # Note:
    ///
    /// Mapping logic is currently only implemented for the most common subtable
    /// formats.
    pub fn map_codepoint(&self, codepoint: impl Into<u32>) -> Option<GlyphId> {
        let codepoint = codepoint.into();
        for record in self.encoding_records() {
            if let Ok(subtable) = record.subtable(self.offset_data()) {
                if let Some(gid) = match subtable {
                    CmapSubtable::Format4(format4) => format4.map_codepoint(codepoint),
                    CmapSubtable::Format12(format12) => format12.map_codepoint(codepoint),
                    _ => None,
                } {
                    return Some(gid);
                }
            }
        }
        None
    }
}

impl<'a> Cmap4<'a> {
    /// Maps a codepoint to a nominal glyph identifier.
    pub fn map_codepoint(&self, codepoint: impl Into<u32>) -> Option<GlyphId> {
        let codepoint = codepoint.into();
        if codepoint > 0xFFFF {
            return None;
        }
        let codepoint = codepoint as u16;
        let mut lo = 0;
        let mut hi = self.seg_count_x2() as usize / 2;
        let start_codes = self.start_code();
        let end_codes = self.end_code();
        while lo < hi {
            let i = (lo + hi) / 2;
            let start_code = start_codes.get(i)?.get();
            if codepoint < start_code {
                hi = i;
            } else if codepoint > end_codes.get(i)?.get() {
                lo = i + 1;
            } else {
                return self.lookup_glyph_id(codepoint, i, start_code);
            }
        }
        None
    }

    /// Returns an iterator over all (codepoint, glyph identifier) pairs
    /// in the subtable.
    pub fn iter(&self) -> Cmap4Iter<'a> {
        Cmap4Iter::new(self.clone())
    }

    /// Does the final phase of glyph id lookup.
    ///
    /// Shared between Self::map and Cmap4Iter.
    fn lookup_glyph_id(&self, codepoint: u16, index: usize, start_code: u16) -> Option<GlyphId> {
        let deltas = self.id_delta();
        let range_offsets = self.id_range_offsets();
        let delta = deltas.get(index)?.get() as i32;
        let range_offset = range_offsets.get(index)?.get() as usize;
        if range_offset == 0 {
            return Some(GlyphId::new((codepoint as i32 + delta) as u16));
        }
        let mut offset = range_offset / 2 + (codepoint - start_code) as usize;
        offset = offset.saturating_sub(range_offsets.len() - index);
        let gid = self.glyph_id_array().get(offset)?.get();
        (gid != 0).then_some(GlyphId::new((gid as i32 + delta) as u16))
    }

    /// Returns the [start_code, end_code] range at the given index.
    fn code_range(&self, index: usize) -> Option<Range<u32>> {
        // Extend to u32 to ensure we don't overflow on the end + 1 bound
        // below.
        let start = self.start_code().get(index)?.get() as u32;
        let end = self.end_code().get(index)?.get() as u32;
        // Use end + 1 here because the range in the table is inclusive
        Some(start..end + 1)
    }
}

/// Iterator over all (codepoint, glyph identifier) pairs in
/// the subtable.
#[derive(Clone)]
pub struct Cmap4Iter<'a> {
    subtable: Cmap4<'a>,
    cur_range: std::ops::Range<u32>,
    cur_start_code: u16,
    cur_range_ix: usize,
}

impl<'a> Cmap4Iter<'a> {
    fn new(subtable: Cmap4<'a>) -> Self {
        let cur_range = subtable.code_range(0).unwrap_or_default();
        let cur_start_code = cur_range.start as u16;
        Self {
            subtable,
            cur_range,
            cur_start_code,
            cur_range_ix: 0,
        }
    }
}

impl<'a> Iterator for Cmap4Iter<'a> {
    type Item = (u32, GlyphId);

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let Some(codepoint) = self.cur_range.next() {
                let glyph_id = self.subtable.lookup_glyph_id(
                    codepoint as u16,
                    self.cur_range_ix,
                    self.cur_start_code,
                )?;
                // The table might explicitly map some codepoints to 0. Avoid
                // returning those here.
                if glyph_id == GlyphId::NOTDEF {
                    continue;
                }
                return Some((codepoint, glyph_id));
            } else {
                self.cur_range_ix += 1;
                self.cur_range = self.subtable.code_range(self.cur_range_ix)?;
                self.cur_start_code = self.cur_range.start as u16;
            }
        }
    }
}

impl<'a> Cmap12<'a> {
    /// Maps a codepoint to a nominal glyph identifier.
    pub fn map_codepoint(&self, codepoint: impl Into<u32>) -> Option<GlyphId> {
        let codepoint = codepoint.into();
        let groups = self.groups();
        let mut lo = 0;
        let mut hi = groups.len();
        while lo < hi {
            let i = (lo + hi) / 2;
            let group = groups.get(i)?;
            if codepoint < group.start_char_code() {
                hi = i;
            } else if codepoint > group.end_char_code() {
                lo = i + 1;
            } else {
                return Some(self.lookup_glyph_id(
                    codepoint,
                    group.start_char_code(),
                    group.start_glyph_id(),
                ));
            }
        }
        None
    }

    /// Returns an iterator over all (codepoint, glyph identifier) pairs
    /// in the subtable.
    pub fn iter(&self) -> Cmap12Iter<'a> {
        Cmap12Iter::new(self.clone())
    }

    /// Does the final phase of glyph id lookup.
    ///
    /// Shared between Self::map and Cmap12Iter.
    fn lookup_glyph_id(
        &self,
        codepoint: u32,
        start_char_code: u32,
        start_glyph_id: u32,
    ) -> GlyphId {
        GlyphId::new(start_glyph_id.wrapping_add(codepoint.wrapping_sub(start_char_code)) as u16)
    }

    /// Returns the codepoint range and start glyph id for the group
    /// at the given index.
    fn group(&self, index: usize) -> Option<(Range<u32>, u32)> {
        let group = self.groups().get(index)?;
        Some((
            // Use + 1 here because the group range is inclusive
            group.start_char_code()..group.end_char_code() + 1,
            group.start_glyph_id(),
        ))
    }
}

/// Iterator over all (codepoint, glyph identifier) pairs in
/// the subtable.
#[derive(Clone)]
pub struct Cmap12Iter<'a> {
    subtable: Cmap12<'a>,
    cur_range: Range<u32>,
    cur_start_code: u32,
    cur_start_glyph_id: u32,
    cur_range_ix: usize,
}

impl<'a> Cmap12Iter<'a> {
    fn new(subtable: Cmap12<'a>) -> Self {
        let (cur_range, cur_start_glyph_id) = subtable.group(0).unwrap_or_default();
        let cur_start_code = cur_range.start;
        Self {
            subtable,
            cur_range,
            cur_start_code,
            cur_start_glyph_id,
            cur_range_ix: 0,
        }
    }
}

impl<'a> Iterator for Cmap12Iter<'a> {
    type Item = (u32, GlyphId);

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let Some(codepoint) = self.cur_range.next() {
                let glyph_id = self.subtable.lookup_glyph_id(
                    codepoint,
                    self.cur_start_code,
                    self.cur_start_glyph_id,
                );
                // The table might explicitly map some codepoints to 0. Avoid
                // returning those here.
                if glyph_id == GlyphId::NOTDEF {
                    continue;
                }
                return Some((codepoint, glyph_id));
            } else {
                self.cur_range_ix += 1;
                (self.cur_range, self.cur_start_glyph_id) =
                    self.subtable.group(self.cur_range_ix)?;
                self.cur_start_code = self.cur_range.start;
            }
        }
    }
}

impl<'a> Cmap14<'a> {
    /// Maps a codepoint and variation selector to a nominal glyph identifier.
    pub fn map_variant(
        &self,
        codepoint: impl Into<u32>,
        selector: impl Into<u32>,
    ) -> Option<MapVariant> {
        let codepoint = codepoint.into();
        let selector = selector.into();
        let selector_records = self.var_selector();
        // Variation selector records are sorted in order of var_selector. Binary search to find
        // the appropriate record.
        let selector_record = selector_records
            .binary_search_by(|rec| {
                let rec_selector: u32 = rec.var_selector().into();
                rec_selector.cmp(&selector)
            })
            .ok()
            .and_then(|idx| selector_records.get(idx))?;
        // If a default UVS table is present in this selector record, binary search on the ranges
        // (start_unicode_value, start_unicode_value + additional_count) to find the requested codepoint.
        // If found, ignore the selector and return a value indicating that the default cmap mapping
        // should be used.
        if let Some(Ok(default_uvs)) = selector_record.default_uvs(self.offset_data()) {
            use core::cmp::Ordering;
            let found_default_uvs = default_uvs
                .ranges()
                .binary_search_by(|range| {
                    let start = range.start_unicode_value().into();
                    if codepoint < start {
                        Ordering::Greater
                    } else if codepoint > (start + range.additional_count() as u32) {
                        Ordering::Less
                    } else {
                        Ordering::Equal
                    }
                })
                .is_ok();
            if found_default_uvs {
                return Some(MapVariant::UseDefault);
            }
        }
        // Binary search the non-default UVS table if present. This maps codepoint+selector to a variant glyph.
        let non_default_uvs = selector_record.non_default_uvs(self.offset_data())?.ok()?;
        let mapping = non_default_uvs.uvs_mapping();
        let ix = mapping
            .binary_search_by(|map| {
                let map_codepoint: u32 = map.unicode_value().into();
                map_codepoint.cmp(&codepoint)
            })
            .ok()?;
        Some(MapVariant::Variant(GlyphId::new(
            mapping.get(ix)?.glyph_id(),
        )))
    }

    /// Returns an iterator over all (codepoint, selector, mapping variant)
    /// triples in the subtable.
    pub fn iter(&self) -> Cmap14Iter<'a> {
        Cmap14Iter::new(self.clone())
    }

    fn selector(
        &self,
        index: usize,
    ) -> (
        Option<VariationSelector>,
        Option<DefaultUvs<'a>>,
        Option<NonDefaultUvs<'a>>,
    ) {
        let selector = self.var_selector().get(index).cloned();
        let default_uvs = selector.as_ref().and_then(|selector| {
            selector
                .default_uvs(self.offset_data())
                .transpose()
                .ok()
                .flatten()
        });
        let non_default_uvs = selector.as_ref().and_then(|selector| {
            selector
                .non_default_uvs(self.offset_data())
                .transpose()
                .ok()
                .flatten()
        });
        (selector, default_uvs, non_default_uvs)
    }
}

/// Iterator over all (codepoint, selector, mapping variant) triples
/// in the subtable.
#[derive(Clone)]
pub struct Cmap14Iter<'a> {
    subtable: Cmap14<'a>,
    selector_record: Option<VariationSelector>,
    default_uvs: Option<DefaultUvsIter<'a>>,
    non_default_uvs: Option<NonDefaultUvsIter<'a>>,
    cur_selector_ix: usize,
}

impl<'a> Cmap14Iter<'a> {
    fn new(subtable: Cmap14<'a>) -> Self {
        let (selector_record, default_uvs, non_default_uvs) = subtable.selector(0);
        Self {
            subtable,
            selector_record,
            default_uvs: default_uvs.map(DefaultUvsIter::new),
            non_default_uvs: non_default_uvs.map(NonDefaultUvsIter::new),
            cur_selector_ix: 0,
        }
    }
}

impl<'a> Iterator for Cmap14Iter<'a> {
    type Item = (u32, u32, MapVariant);

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let selector_record = self.selector_record.as_ref()?;
            let selector: u32 = selector_record.var_selector().into();
            if let Some(default_uvs) = self.default_uvs.as_mut() {
                if let Some(codepoint) = default_uvs.next() {
                    return Some((codepoint, selector, MapVariant::UseDefault));
                }
            }
            if let Some(non_default_uvs) = self.non_default_uvs.as_mut() {
                if let Some((codepoint, variant)) = non_default_uvs.next() {
                    return Some((codepoint, selector, MapVariant::Variant(variant)));
                }
            }
            self.cur_selector_ix += 1;
            let (selector_record, default_uvs, non_default_uvs) =
                self.subtable.selector(self.cur_selector_ix);
            self.selector_record = selector_record;
            self.default_uvs = default_uvs.map(DefaultUvsIter::new);
            self.non_default_uvs = non_default_uvs.map(NonDefaultUvsIter::new);
        }
    }
}

#[derive(Clone)]
struct DefaultUvsIter<'a> {
    ranges: std::slice::Iter<'a, UnicodeRange>,
    cur_range: Range<u32>,
}

impl<'a> DefaultUvsIter<'a> {
    fn new(ranges: DefaultUvs<'a>) -> Self {
        let mut ranges = ranges.ranges().iter();
        let cur_range = if let Some(range) = ranges.next() {
            let start: u32 = range.start_unicode_value().into();
            let end = start + range.additional_count() as u32 + 1;
            start..end
        } else {
            0..0
        };
        Self { ranges, cur_range }
    }
}

impl<'a> Iterator for DefaultUvsIter<'a> {
    type Item = u32;

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            if let Some(codepoint) = self.cur_range.next() {
                return Some(codepoint);
            }
            let range = self.ranges.next()?;
            let start: u32 = range.start_unicode_value().into();
            let end = start + range.additional_count() as u32 + 1;
            self.cur_range = start..end;
        }
    }
}

#[derive(Clone)]
struct NonDefaultUvsIter<'a> {
    iter: std::slice::Iter<'a, UvsMapping>,
}

impl<'a> NonDefaultUvsIter<'a> {
    fn new(uvs: NonDefaultUvs<'a>) -> Self {
        Self {
            iter: uvs.uvs_mapping().iter(),
        }
    }
}

impl<'a> Iterator for NonDefaultUvsIter<'a> {
    type Item = (u32, GlyphId);

    fn next(&mut self) -> Option<Self::Item> {
        let mapping = self.iter.next()?;
        let codepoint: u32 = mapping.unicode_value().into();
        let glyph_id = GlyphId::new(mapping.glyph_id());
        Some((codepoint, glyph_id))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{FontRef, GlyphId, TableProvider};

    #[test]
    fn map_codepoints() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let cmap = font.cmap().unwrap();
        assert_eq!(cmap.map_codepoint('A'), Some(GlyphId::new(1)));
        assert_eq!(cmap.map_codepoint('À'), Some(GlyphId::new(2)));
        assert_eq!(cmap.map_codepoint('`'), Some(GlyphId::new(3)));
        assert_eq!(cmap.map_codepoint('B'), None);

        let font = FontRef::new(font_test_data::SIMPLE_GLYF).unwrap();
        let cmap = font.cmap().unwrap();
        assert_eq!(cmap.map_codepoint(' '), Some(GlyphId::new(1)));
        assert_eq!(cmap.map_codepoint(0xE_u32), Some(GlyphId::new(2)));
        assert_eq!(cmap.map_codepoint('B'), None);
    }

    #[test]
    fn map_variants() {
        use super::MapVariant::*;
        let font = FontRef::new(font_test_data::CMAP14_FONT1).unwrap();
        let cmap = font.cmap().unwrap();
        let cmap14 = find_cmap14(&cmap).unwrap();
        let selector = '\u{e0100}';
        assert_eq!(cmap14.map_variant('a', selector), None);
        assert_eq!(cmap14.map_variant('\u{4e00}', selector), Some(UseDefault));
        assert_eq!(cmap14.map_variant('\u{4e06}', selector), Some(UseDefault));
        assert_eq!(
            cmap14.map_variant('\u{4e08}', selector),
            Some(Variant(GlyphId::new(25)))
        );
        assert_eq!(
            cmap14.map_variant('\u{4e09}', selector),
            Some(Variant(GlyphId::new(26)))
        );
    }

    #[test]
    fn cmap4_iter() {
        let font = FontRef::new(font_test_data::VAZIRMATN_VAR).unwrap();
        let cmap4 = find_cmap4(&font.cmap().unwrap()).unwrap();
        let mut count = 0;
        for (codepoint, glyph_id) in cmap4.iter() {
            assert_eq!(cmap4.map_codepoint(codepoint), Some(glyph_id));
            count += 1;
        }
        assert_eq!(count, 3);
        let font = FontRef::new(font_test_data::SIMPLE_GLYF).unwrap();
        let cmap4 = find_cmap4(&font.cmap().unwrap()).unwrap();
        let mut count = 0;
        for (codepoint, glyph_id) in cmap4.iter() {
            assert_eq!(cmap4.map_codepoint(codepoint), Some(glyph_id));
            count += 1;
        }
        assert_eq!(count, 2);
    }

    #[test]
    fn cmap12_iter() {
        let font = FontRef::new(font_test_data::CMAP12_FONT1).unwrap();
        let cmap12 = find_cmap12(&font.cmap().unwrap()).unwrap();
        let mut count = 0;
        for (codepoint, glyph_id) in cmap12.iter() {
            assert_eq!(cmap12.map_codepoint(codepoint), Some(glyph_id));
            count += 1;
        }
        assert_eq!(count, 10);
    }

    #[test]
    fn cmap14_iter() {
        let font = FontRef::new(font_test_data::CMAP14_FONT1).unwrap();
        let cmap14 = find_cmap14(&font.cmap().unwrap()).unwrap();
        let mut count = 0;
        for (codepoint, selector, mapping) in cmap14.iter() {
            assert_eq!(cmap14.map_variant(codepoint, selector), Some(mapping));
            count += 1;
        }
        assert_eq!(count, 7);
    }

    fn find_cmap4<'a>(cmap: &Cmap<'a>) -> Option<Cmap4<'a>> {
        cmap.encoding_records()
            .iter()
            .filter_map(|record| record.subtable(cmap.offset_data()).ok())
            .find_map(|subtable| match subtable {
                CmapSubtable::Format4(cmap4) => Some(cmap4),
                _ => None,
            })
    }

    fn find_cmap12<'a>(cmap: &Cmap<'a>) -> Option<Cmap12<'a>> {
        cmap.encoding_records()
            .iter()
            .filter_map(|record| record.subtable(cmap.offset_data()).ok())
            .find_map(|subtable| match subtable {
                CmapSubtable::Format12(cmap12) => Some(cmap12),
                _ => None,
            })
    }

    fn find_cmap14<'a>(cmap: &Cmap<'a>) -> Option<Cmap14<'a>> {
        cmap.encoding_records()
            .iter()
            .filter_map(|record| record.subtable(cmap.offset_data()).ok())
            .find_map(|subtable| match subtable {
                CmapSubtable::Format14(cmap14) => Some(cmap14),
                _ => None,
            })
    }
}
