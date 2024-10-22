//! The [cmap](https://docs.microsoft.com/en-us/typography/opentype/spec/cmap) table

include!("../../generated/generated_cmap.rs");

#[cfg(feature = "std")]
use crate::collections::IntSet;
use std::ops::{Range, RangeInclusive};

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

    #[cfg(feature = "std")]
    pub fn closure_glyphs(&self, unicodes: &IntSet<u32>, glyph_set: &mut IntSet<GlyphId>) {
        for record in self.encoding_records() {
            if let Ok(subtable) = record.subtable(self.offset_data()) {
                match subtable {
                    CmapSubtable::Format14(format14) => {
                        format14.closure_glyphs(unicodes, glyph_set);
                        return;
                    }
                    _ => {
                        continue;
                    }
                }
            }
        }
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
            return Some(GlyphId::from((codepoint as i32 + delta) as u16));
        }
        let mut offset = range_offset / 2 + (codepoint - start_code) as usize;
        offset = offset.saturating_sub(range_offsets.len() - index);
        let gid = self.glyph_id_array().get(offset)?.get();
        (gid != 0).then_some(GlyphId::from((gid as i32 + delta) as u16))
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
    cur_range: Range<u32>,
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
                let Some(glyph_id) = self.subtable.lookup_glyph_id(
                    codepoint as u16,
                    self.cur_range_ix,
                    self.cur_start_code,
                ) else {
                    continue;
                };
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
        GlyphId::new(start_glyph_id.wrapping_add(codepoint.wrapping_sub(start_char_code)))
    }

    /// Returns the codepoint range and start glyph id for the group
    /// at the given index.
    fn group(&self, index: usize) -> Option<Cmap12Group> {
        let group = self.groups().get(index)?;
        let start_code = group.start_char_code();
        // Limit to the valid range of Unicode characters
        // per https://github.com/googlefonts/fontations/issues/952#issuecomment-2161510184
        let end_code = group.end_char_code().min(char::MAX as u32);
        Some(Cmap12Group {
            range: start_code..=end_code,
            start_code,
            start_glyph_id: group.start_glyph_id(),
        })
    }
}

#[derive(Clone)]
struct Cmap12Group {
    range: RangeInclusive<u32>,
    start_code: u32,
    start_glyph_id: u32,
}

/// Iterator over all (codepoint, glyph identifier) pairs in
/// the subtable.
#[derive(Clone)]
pub struct Cmap12Iter<'a> {
    subtable: Cmap12<'a>,
    cur_group: Option<Cmap12Group>,
    cur_group_ix: usize,
}

impl<'a> Cmap12Iter<'a> {
    fn new(subtable: Cmap12<'a>) -> Self {
        let cur_group = subtable.group(0);
        Self {
            subtable,
            cur_group,
            cur_group_ix: 0,
        }
    }
}

impl<'a> Iterator for Cmap12Iter<'a> {
    type Item = (u32, GlyphId);

    fn next(&mut self) -> Option<Self::Item> {
        loop {
            let group = self.cur_group.as_mut()?;
            if let Some(codepoint) = group.range.next() {
                let glyph_id = self.subtable.lookup_glyph_id(
                    codepoint,
                    group.start_code,
                    group.start_glyph_id,
                );
                // The table might explicitly map some codepoints to 0. Avoid
                // returning those here.
                if glyph_id == GlyphId::NOTDEF {
                    continue;
                }
                return Some((codepoint, glyph_id));
            } else {
                self.cur_group_ix += 1;
                let mut next_group = self.subtable.group(self.cur_group_ix)?;
                // Groups should be in order and non-overlapping so make sure
                // that the start code of next group is at least
                // current_end + 1.
                // This ensures we only ever generate a maximum of
                // char::MAX + 1 results.
                if next_group.range.start() <= group.range.end() {
                    next_group.range = *group.range.end() + 1..=*next_group.range.end();
                }
                self.cur_group = Some(next_group);
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
        Some(MapVariant::Variant(GlyphId::from(
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

    #[cfg(feature = "std")]
    pub fn closure_glyphs(&self, unicodes: &IntSet<u32>, glyph_set: &mut IntSet<GlyphId>) {
        for selector in self.var_selector() {
            if let Some(non_default_uvs) = selector
                .non_default_uvs(self.offset_data())
                .transpose()
                .ok()
                .flatten()
            {
                glyph_set.extend(
                    non_default_uvs
                        .uvs_mapping()
                        .iter()
                        .filter(|m| unicodes.contains(m.unicode_value().to_u32()))
                        .map(|m| m.glyph_id().into()),
                );
            }
        }
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
                    return Some((codepoint, selector, MapVariant::Variant(variant.into())));
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
    type Item = (u32, GlyphId16);

    fn next(&mut self) -> Option<Self::Item> {
        let mapping = self.iter.next()?;
        let codepoint: u32 = mapping.unicode_value().into();
        let glyph_id = GlyphId16::new(mapping.glyph_id());
        Some((codepoint, glyph_id))
    }
}

#[cfg(test)]
mod tests {
    use super::*;
    use crate::{test_helpers::BeBuffer, FontRef, GlyphId, TableProvider};

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
    #[cfg(feature = "std")]
    fn cmap14_closure_glyphs() {
        let font = FontRef::new(font_test_data::CMAP14_FONT1).unwrap();
        let cmap = font.cmap().unwrap();
        let mut unicodes = IntSet::empty();
        unicodes.insert(0x4e08_u32);

        let mut glyph_set = IntSet::empty();
        glyph_set.insert(GlyphId::new(18));
        cmap.closure_glyphs(&unicodes, &mut glyph_set);

        assert_eq!(glyph_set.len(), 2);
        assert!(glyph_set.contains(GlyphId::new(18)));
        assert!(glyph_set.contains(GlyphId::new(25)));
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

    // Make sure we don't bail early when iterating ranges with holes.
    // Encountered with Gentium Basic and Gentium Basic Book.
    // See <https://github.com/googlefonts/fontations/issues/897>
    #[test]
    fn cmap4_iter_sparse_range() {
        #[rustfmt::skip]
        let cmap4_data: &[u16] = &[
            // format, length, lang
            4, 0, 0,
            // segCountX2
            4, 
            // bin search data
            0, 0, 0,
            // end code
            262, 0xFFFF, 
            // reserved pad
            0,
            // start code
            259, 0xFFFF,
            // id delta
            0, 1, 
            // id range offset
            4, 0,
            // glyph ids
            236, 0, 0, 326,
        ];
        let mut buf = BeBuffer::new();
        for &word in cmap4_data {
            buf = buf.push(word);
        }
        let cmap4 = Cmap4::read(FontData::new(&buf)).unwrap();
        let mappings = cmap4
            .iter()
            .map(|(ch, gid)| (ch, gid.to_u32()))
            .collect::<Vec<_>>();
        assert_eq!(mappings, &[(259, 236), (262, 326)]);
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

    // oss-fuzz: detected integer addition overflow in Cmap12::group()
    // ref: https://oss-fuzz.com/testcase-detail/5141969742397440
    // and https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=69547
    #[test]
    fn cmap12_iter_avoid_overflow() {
        let test_case = &[
            79, 84, 84, 79, 0, 5, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0, 10, 32, 32, 32, 32, 32, 32, 32,
            99, 109, 97, 112, 32, 32, 32, 32, 0, 0, 0, 33, 0, 0, 0, 84, 32, 32, 32, 32, 32, 32, 0,
            12, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0, 0, 0, 2, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 255, 255, 255, 255, 255, 255, 255, 32, 32, 32, 32, 0,
            0, 32, 32, 0, 0, 0, 33,
        ];
        let font = FontRef::new(test_case).unwrap();
        let cmap = font.cmap().unwrap();
        let cmap12 = find_cmap12(&cmap).unwrap();
        let _ = cmap12.iter().count();
    }

    // oss-fuzz: timeout in Cmap12Iter
    // ref: https://oss-fuzz.com/testcase-detail/4628971063934976
    // and https://bugs.chromium.org/p/oss-fuzz/issues/detail?id=69540
    #[test]
    fn cmap12_iter_avoid_timeout() {
        let test_case = &[
            0, 1, 0, 0, 0, 5, 0, 1, 0, 0, 5, 0, 0, 0, 0, 0, 0, 0, 0, 48, 0, 93, 0, 0, 3, 8, 0, 0,
            151, 3, 0, 0, 0, 0, 0, 0, 0, 64, 255, 103, 5, 7, 221, 0, 99, 109, 97, 112, 0, 0, 3, 0,
            0, 0, 0, 2, 0, 0, 0, 97, 97, 97, 159, 158, 158, 149, 0, 12, 255, 255, 249, 2, 0, 1, 0,
            0, 0, 23, 0, 0, 0, 1, 0, 0, 0, 170, 79, 84, 84, 79, 0, 5, 5, 0, 1, 0, 0, 5, 5, 5, 5,
            48, 5, 5, 5, 5, 0, 4, 0, 1, 0, 0, 0, 0, 0, 0, 0, 0, 255, 255, 255, 255, 0, 0, 33, 0, 5,
            3, 5, 5, 5, 5, 74, 5, 255, 255, 32, 1, 5, 44, 0, 0, 10, 116, 0, 33, 0, 0, 0, 102, 0, 0,
            0, 0, 0, 0, 0, 0, 78, 0, 0, 0, 0, 0, 0, 0, 0, 0, 33, 0, 0, 87, 250, 181, 250, 250, 159,
            0, 4, 0, 0, 0, 0, 0, 99, 109, 97, 112, 4, 64, 138, 0, 0, 0, 0, 33, 0, 0, 3, 0, 0, 0,
            102, 0, 0, 0, 0, 0, 0, 0, 0, 78, 0, 0, 0, 0, 0, 255, 255, 96, 0, 0, 0, 12, 32, 0, 1, 0,
            0, 5, 5, 97, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 0, 149, 97, 0, 0, 4, 0, 0,
            128, 5, 53, 37, 5, 5, 44, 5, 5, 0, 3, 5,
        ];
        let font = FontRef::new(test_case).unwrap();
        let cmap = font.cmap().unwrap();
        // ranges: [SequentialMapGroup { start_char_code: 170, end_char_code: 1330926671, start_glyph_id: 328960 }]
        let cmap12 = find_cmap12(&cmap).unwrap();
        assert!(cmap12.iter().count() <= char::MAX as usize + 1);
    }

    #[test]
    fn cmap12_iter_range_clamping() {
        let test_case = &[
            79, 84, 84, 79, 0, 5, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32,
            32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0, 10, 32, 32, 32, 32, 32, 32, 32,
            99, 109, 97, 112, 32, 32, 32, 32, 0, 0, 0, 33, 0, 0, 0, 84, 32, 32, 32, 32, 32, 32, 0,
            12, 32, 32, 32, 32, 32, 32, 32, 32, 32, 32, 0, 0, 0, 2, 0, 0, 0, 0, 0, 255, 255, 255,
            32, 32, 32, 32, 0, 0, 0, 255, 255, 255, 255, 255, 255, 255, 255, 32, 32, 32, 32, 0, 0,
            32, 32, 0, 0, 0, 33,
        ];
        let font = FontRef::new(test_case).unwrap();
        let cmap = font.cmap().unwrap();
        let cmap12 = find_cmap12(&cmap).unwrap();
        let ranges = cmap12
            .groups()
            .iter()
            .map(|group| (group.start_char_code(), group.end_char_code()))
            .collect::<Vec<_>>();
        // These groups overlap and extend to the whole u32 range
        assert_eq!(ranges, &[(0, 16777215), (255, u32::MAX)]);
        // But we produce at most char::MAX + 1 results
        assert!(cmap12.iter().count() <= char::MAX as usize + 1);
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
