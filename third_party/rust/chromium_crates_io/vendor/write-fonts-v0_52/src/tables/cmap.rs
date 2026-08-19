//! the [cmap] table
//!
//! [cmap]: https://docs.microsoft.com/en-us/typography/opentype/spec/cmap

include!("../../generated/generated_cmap.rs");

use std::collections::HashMap;

use crate::search_range::SearchRange;

// https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#windows-platform-platform-id--3
const WINDOWS_BMP_ENCODING: u16 = 1;
const WINDOWS_FULL_REPERTOIRE_ENCODING: u16 = 10;

// https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#unicode-platform-platform-id--0
const UNICODE_BMP_ENCODING: u16 = 3;
const UNICODE_FULL_REPERTOIRE_ENCODING: u16 = 4;

impl CmapSubtable {
    /// Create a new format 4 subtable
    ///
    /// Returns `None` if none of the input chars are in the BMP (i.e. have
    /// codepoints <= 0xFFFF.)
    ///
    /// Invariants:
    ///
    /// - Inputs must be sorted and deduplicated.
    /// - All `GlyphId`s must be 16-bit
    fn create_format_4(mappings: &[(char, GlyphId)]) -> Option<Self> {
        let mut end_code = Vec::with_capacity(mappings.len() + 1);
        let mut start_code = Vec::with_capacity(mappings.len() + 1);
        let mut id_deltas = Vec::with_capacity(mappings.len() + 1);
        let mut id_range_offsets = Vec::with_capacity(mappings.len() + 1);
        let mut glyph_ids = Vec::new();

        let segments = Format4SegmentComputer::new(mappings).compute();
        assert!(mappings.iter().all(|(_, g)| g.to_u32() <= 0xFFFF));
        if segments.is_empty() {
            // no chars in BMP
            return None;
        }
        let n_segments = segments.len() + 1;
        for (i, segment) in segments.into_iter().enumerate() {
            let start = mappings[segment.start_ix].0;
            let end = mappings[segment.end_ix].0;
            start_code.push(start as u32 as u16);
            end_code.push(end as u32 as u16);
            if let Some(delta) = segment.id_delta {
                // "The idDelta arithmetic is modulo 65536":
                let delta = i16::try_from(delta)
                    .unwrap_or_else(|_| delta.rem_euclid(0x10000).try_into().unwrap());
                id_deltas.push(delta);
                id_range_offsets.push(0u16);
            } else {
                // if the deltas for a range are not identical, we rely on the
                // explicit glyph_ids array.
                //
                // The logic here is based on the memory layout of the table:
                // because the glyph_id array follows the id_range_offsets array,
                // the id_range_offsets array essentially stores a memory offset.
                let current_n_ids = glyph_ids.len();
                let n_following_segments = n_segments - i;
                // number of bytes from the id_range_offset value to the glyph id
                // for this segment, in the glyph_ids array
                let id_range_offset = (n_following_segments + current_n_ids) * u16::RAW_BYTE_LEN;
                id_deltas.push(0);
                id_range_offsets.push(id_range_offset.try_into().unwrap());
                glyph_ids.extend(
                    mappings[segment.start_ix..=segment.end_ix]
                        .iter()
                        .map(|(_, gid)| u16::try_from(gid.to_u32()).expect("checked before now")),
                )
            }
        }

        // add the final segment:
        end_code.push(0xFFFF);
        start_code.push(0xFFFF);
        id_deltas.push(1);
        id_range_offsets.push(0);

        Some(Self::format_4(
            0,
            end_code,
            start_code,
            id_deltas,
            id_range_offsets,
            glyph_ids,
        ))
    }

    /// Create a new format 12 `CmapSubtable` from a list of `(char, GlyphId)` pairs.
    ///
    /// The pairs are expected to be already sorted by chars.
    /// In case of duplicate chars, the last one wins.
    fn create_format_12(mappings: &[(char, GlyphId)]) -> Self {
        let (mut char_codes, gids): (Vec<u32>, Vec<u32>) = mappings
            .iter()
            .map(|(cp, gid)| (*cp as u32, gid.to_u32()))
            .unzip();
        let cmap: HashMap<_, _> = char_codes.iter().cloned().zip(gids).collect();
        char_codes.dedup();

        // we know we have at least one non-BMP char_code > 0xFFFF so unwrap is safe
        let mut start_char_code = *char_codes.first().unwrap();
        let mut start_glyph_id = cmap[&start_char_code];
        let mut last_glyph_id = start_glyph_id.wrapping_sub(1);
        let mut last_char_code = start_char_code.wrapping_sub(1);
        let mut groups = Vec::new();
        for char_code in char_codes {
            let glyph_id = cmap[&char_code];
            if glyph_id != last_glyph_id.wrapping_add(1)
                || char_code != last_char_code.wrapping_add(1)
            {
                groups.push((start_char_code, last_char_code, start_glyph_id));
                start_char_code = char_code;
                start_glyph_id = glyph_id;
            }
            last_glyph_id = glyph_id;
            last_char_code = char_code;
        }
        groups.push((start_char_code, last_char_code, start_glyph_id));

        let seq_map_groups = groups
            .into_iter()
            .map(|(start_char, end_char, gid)| SequentialMapGroup::new(start_char, end_char, gid))
            .collect::<Vec<_>>();
        CmapSubtable::format_12(
            0, // 'lang' set to zero for all 'cmap' subtables whose platform IDs are other than Macintosh
            seq_map_groups,
        )
    }
}

/// A conflicting Cmap definition, one char is mapped to multiple distinct GlyphIds.
///
/// If there are multiple conflicting mappings, one is chosen arbitrarily.
/// gid1 is less than gid2.
#[derive(Debug, Clone, PartialEq, Eq)]
pub struct CmapConflict {
    ch: char,
    gid1: GlyphId,
    gid2: GlyphId,
}

impl std::fmt::Display for CmapConflict {
    fn fmt(&self, f: &mut core::fmt::Formatter<'_>) -> core::fmt::Result {
        let ch32 = self.ch as u32;
        write!(
            f,
            "Cannot map {:?} (U+{ch32:04X}) to two different glyph ids: {} and {}",
            self.ch, self.gid1, self.gid2
        )
    }
}

impl std::error::Error for CmapConflict {}

impl Cmap {
    /// Generates a ['cmap'] that is expected to work in most modern environments.
    ///
    /// The input is not required to be sorted.
    ///
    /// This emits [format 4] and [format 12] subtables, respectively for the
    /// Basic Multilingual Plane and Full Unicode Repertoire.
    ///
    /// Also see: <https://learn.microsoft.com/en-us/typography/opentype/spec/recom#cmap-table>
    ///
    /// [`cmap`]: https://learn.microsoft.com/en-us/typography/opentype/spec/cmap
    /// [format 4]: https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-4-segment-mapping-to-delta-values
    /// [format 12]: https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-12-segmented-coverage
    pub fn from_mappings(
        mappings: impl IntoIterator<Item = (char, GlyphId)>,
    ) -> Result<Cmap, CmapConflict> {
        let mut mappings: Vec<_> = mappings.into_iter().collect();
        mappings.sort();
        mappings.dedup();
        if let Some((ch, gid1, gid2)) =
            mappings
                .iter()
                .zip(mappings.iter().skip(1))
                .find_map(|((c1, g1), (c2, g2))| {
                    (c1 == c2 && g1 != g2).then(|| (*c1, *g1.min(g2), *g1.max(g2)))
                })
        {
            return Err(CmapConflict { ch, gid1, gid2 });
        }

        let mut uni_records = Vec::new(); // platform 0
        let mut win_records = Vec::new(); // platform 3

        // if there are characters in the Unicode Basic Multilingual Plane (U+0000 to U+FFFF)
        // we need to emit format 4 subtables
        let bmp_subtable = CmapSubtable::create_format_4(&mappings);
        if let Some(bmp_subtable) = bmp_subtable {
            // Absent a strong signal to do otherwise, match fontmake/fonttools
            // Since both Windows and Unicode platform tables use the same subtable they are
            // almost entirely byte-shared
            // See https://github.com/googlefonts/fontmake-rs/issues/251
            uni_records.push(EncodingRecord::new(
                PlatformId::Unicode,
                UNICODE_BMP_ENCODING,
                bmp_subtable.clone(),
            ));
            win_records.push(EncodingRecord::new(
                PlatformId::Windows,
                WINDOWS_BMP_ENCODING,
                bmp_subtable,
            ));
        }

        // If there are any supplementary-plane characters (U+10000 to U+10FFFF) we also
        // emit format 12 subtables
        if mappings.iter().any(|(cp, _)| *cp > '\u{FFFF}') {
            let full_repertoire_subtable = CmapSubtable::create_format_12(&mappings);
            // format 12 subtables are also going to be byte-shared, just like above
            uni_records.push(EncodingRecord::new(
                PlatformId::Unicode,
                UNICODE_FULL_REPERTOIRE_ENCODING,
                full_repertoire_subtable.clone(),
            ));
            win_records.push(EncodingRecord::new(
                PlatformId::Windows,
                WINDOWS_FULL_REPERTOIRE_ENCODING,
                full_repertoire_subtable,
            ));
        }

        // put encoding records in order of (platform id, encoding id):
        // - Unicode (0), BMP (3)
        // - Unicode (0), full repertoire (4)
        // - Windows (3), BMP (1)
        // - Windows (3), full repertoire (10)
        Ok(Cmap::new(
            uni_records.into_iter().chain(win_records).collect(),
        ))
    }
}

// a helper for computing efficient segments for cmap format 4
struct Format4SegmentComputer<'a> {
    mappings: &'a [(char, GlyphId)],
    /// The start index of the current segment, during iteration
    seg_start: usize,
    /// tracks whether the current segment has ordered gids
    gids_in_order: bool,
}

#[derive(Clone, Copy, Debug)]
struct Format4Segment {
    // indices are into the source mappings
    start_ix: usize,
    end_ix: usize,
    start_char: char,
    end_char: char,
    id_delta: Option<i32>,
}

impl Format4Segment {
    fn len(&self) -> usize {
        self.end_ix - self.start_ix + 1
    }

    // cost in bytes of this segment.
    fn cost(&self) -> usize {
        // a segment always costs 4 u16s (end, start, delta_id, id_range_offset)
        const BASE_COST: usize = 4 * u16::RAW_BYTE_LEN;

        if self.id_delta.is_some() {
            BASE_COST
        } else {
            // and if there is not a common id_delta, we also need to add an item
            // to the glyph_id_array for each char in the segment
            BASE_COST + self.len() * u16::RAW_BYTE_LEN
        }
    }

    /// `true` if we can merge other into self (other must follow self)
    fn can_combine(&self, next: &Self) -> bool {
        self.end_char as u32 + 1 == next.start_char as u32
    }

    /// Return `true` if we should combine this segment with the previous one.
    ///
    /// The case that matters here is when there is a segment with contiguous
    /// GIDs and with a char range that is immediately adjacent to the previous
    /// segment.
    fn should_combine(&self, prev: &Self, next: Option<&Self>) -> bool {
        if !prev.can_combine(self) {
            return false;
        }

        // first we just consider the previous item. If our combined cost
        // is lower than our separate cost, we will merge.
        let combined_cost = prev.combine(self).cost();
        let separate_cost = prev.cost() + self.cost();

        if combined_cost < separate_cost {
            return true;
        }

        // finally, if we are also char-contiguous with the next segment,
        // then by construction it means if we merge now we will also merge
        // with the next segment (since this current gid-contiguous segment
        // is the reason we aren't all one big segment already) and so we need
        // to also check that.
        //
        // Although the implementation is different, the logic is very similar in
        // fonttools: https://github.com/fonttools/fonttools/blob/081d6a27ab8/Lib/fontTools/ttLib/tables/_c_m_a_p.py#L828
        //
        // As an example, consider a segment with 5 contiguous gids.
        //
        // This segment costs 8 bytes to encode; because the gids are contiguous
        // we can use the `id_delta` field to represent them all.
        //
        // As an example, consider the following three segments:
        //
        // chrs [1 2] [3 4 5 6 7] [8 9]
        // GIDs [3 1] [4 5 6 7 8] [2 9]
        // cost   12       8       12
        //
        // the first and last segments each have len == 2. The GIDs are not
        // contiguous, so they have to be encoded individually, which costs
        // 2 bytes each. This means the total cost of these segments is 12:
        // 8-bytes for the segment data, and 4 bytes for the gids.
        //
        // The middle segment has len == 5, but the GIDs are contiguous. This
        // means that we can represent all the gids using the delta_id part of
        // the segment, and encode the whole segment for 8 bytes.
        //
        // If we combine the first two segments, the new segment costs 22:
        // 8 bytes for the segment, and 14 bytes for the 7 glyphs. This is
        // more than the 20 bytes they cost separately.
        //
        // If we combine all three, though, the total cost is 26 (we add two
        // more entries to the glyph_id array), which is better than the 32 bytes
        // they cost separately.
        //
        // (note that we don't need to explicitly combine the next segment;
        // it will happen automatically during the next loop)
        if let Some(next) = next.filter(|next| self.can_combine(next)) {
            let combined_cost = prev.combine(self).combine(next).cost();
            let separate_cost = separate_cost + next.cost();
            return combined_cost < separate_cost;
        }

        false
    }

    /// Combine this segment with one that immediately follows it.
    ///
    /// The caller must ensure that the two segments are contiguous.
    fn combine(&self, next: &Format4Segment) -> Format4Segment {
        assert_eq!(next.start_ix, self.end_ix + 1,);
        Format4Segment {
            start_ix: self.start_ix,
            start_char: self.start_char,
            end_char: next.end_char,
            end_ix: next.end_ix,
            id_delta: None,
        }
    }
}

impl<'a> Format4SegmentComputer<'a> {
    fn new(mappings: &'a [(char, GlyphId)]) -> Self {
        // ignore chars above BMP:
        let mappings = mappings
            .iter()
            .position(|(c, _)| u16::try_from(*c as u32).is_err())
            .map(|bad_idx| &mappings[..bad_idx])
            .unwrap_or(mappings);
        Self {
            mappings,
            seg_start: 0,
            gids_in_order: false,
        }
    }

    /// a convenience method called from our iter in the various cases where
    /// we emit a segment.
    ///
    /// a 'seg_len' of 0 means start == end, e.g. a segment of one glyph.
    fn make_segment(&mut self, seg_len: usize) -> Format4Segment {
        // if start == end, we should always use a delta.
        let use_delta = self.gids_in_order || seg_len == 0;
        let start_ix = self.seg_start;
        let end_ix = self.seg_start + seg_len;
        let start_char = self.mappings[start_ix].0;
        let end_char = self.mappings[end_ix].0;
        let result = Format4Segment {
            start_ix,
            end_ix,
            start_char,
            end_char,
            id_delta: self
                .mappings
                .get(self.seg_start)
                .map(|(cp, gid)| gid.to_u32() as i32 - *cp as u32 as i32)
                .filter(|_| use_delta),
        };
        self.seg_start += seg_len + 1;
        self.gids_in_order = false;
        result
    }

    /// Find the next possible segment.
    ///
    /// A segment _must_ be a contiguous range of chars, but we where such a range
    /// contains subranges that are also contiguous ranges of glyph ids, we will
    /// split those subranges into separate segments.
    fn next_possible_segment(&mut self) -> Option<Format4Segment> {
        if self.seg_start == self.mappings.len() {
            return None;
        }

        let Some(((mut prev_cp, mut prev_gid), rest)) =
            self.mappings[self.seg_start..].split_first()
        else {
            // if this is the last element, make a final segment
            return Some(self.make_segment(0));
        };

        for (i, (cp, gid)) in rest.iter().enumerate() {
            // first: all segments must be a contiguous range of codepoints
            if *cp as u32 != prev_cp as u32 + 1 {
                return Some(self.make_segment(i));
            }
            let next_gid_is_in_order = prev_gid.to_u32() + 1 == gid.to_u32();
            if !next_gid_is_in_order {
                // next: if prev gids were ordered but this one isn't, end prev segment
                if self.gids_in_order {
                    return Some(self.make_segment(i));
                }
            // and the funny case:
            // if gids were not previously ordered but are now:
            // - if i == 0, then this is the first item in a new segment;
            //   set gids_in_order and continue
            // - if i > 0, we need to back up one
            } else if !self.gids_in_order {
                if i == 0 {
                    self.gids_in_order = true;
                } else {
                    return Some(self.make_segment(i - 1));
                }
            }
            prev_cp = *cp;
            prev_gid = *gid;
        }

        // if we're done looping then create the last segment:
        let last_idx = self.mappings.len() - 1;
        Some(self.make_segment(last_idx - self.seg_start))
    }

    /// Compute an efficient set of segments.
    ///
    /// - A segment is a contiguous range of chars.
    /// - If all the chars in a segment share a common delta to their glyph ids,
    ///   we can encode them much more efficiently
    /// - it's possible for a contiguous range of chars to contain a subrange
    ///   that share a common delta, where the overall range does not, e.g.
    ///
    ///   ```text
    ///   [a b c d e f g]
    ///   [9 3 6 7 8 2 1]
    ///   ```
    ///   (here a-g is a range containing the subrange c-e, which have a common
    ///   delta.)
    ///
    /// This leads us to a reasonably intuitive algorithm: we start by greedily
    /// splitting ranges up so we can consider all subranges with common deltas;
    /// then we look at these one at a time, and combine them back together if
    /// doing so saves space.
    ///
    /// This differs from the python, which starts from larger segments and then
    /// subdivides them, but the overall idea is the same.
    ///
    /// <https://github.com/fonttools/fonttools/blob/f1d3e116d54f/Lib/fontTools/ttLib/tables/_c_m_a_p.py#L783>
    fn compute(mut self) -> Vec<Format4Segment> {
        let Some(first) = self.next_possible_segment() else {
            return Default::default();
        };

        let mut result = vec![first];

        // now we want to collect the segments, combining smaller segments where
        // that leads to a size savings.
        let mut next = self.next_possible_segment();

        while let Some(current) = next.take() {
            next = self.next_possible_segment();
            let prev = result.last_mut().unwrap();
            if current.should_combine(prev, next.as_ref()) {
                *prev = prev.combine(&current);
                continue;
            }

            result.push(current);
        }
        result
    }
}

impl Cmap4 {
    fn compute_length(&self) -> u16 {
        // https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-4-segment-mapping-to-delta-values
        // there are always 8 u16 fields
        const FIXED_SIZE: usize = 8 * u16::RAW_BYTE_LEN;
        const PER_SEGMENT_LEN: usize = 4 * u16::RAW_BYTE_LEN;

        let segment_len = self.end_code.len() * PER_SEGMENT_LEN;
        let gid_len = self.glyph_id_array.len() * u16::RAW_BYTE_LEN;

        (FIXED_SIZE + segment_len + gid_len)
            .try_into()
            .expect("cmap4 overflow")
    }

    fn compute_search_range(&self) -> u16 {
        SearchRange::compute(self.end_code.len(), u16::RAW_BYTE_LEN).search_range
    }

    fn compute_entry_selector(&self) -> u16 {
        SearchRange::compute(self.end_code.len(), u16::RAW_BYTE_LEN).entry_selector
    }

    fn compute_range_shift(&self) -> u16 {
        SearchRange::compute(self.end_code.len(), u16::RAW_BYTE_LEN).range_shift
    }
}

impl Cmap12 {
    fn compute_length(&self) -> u32 {
        // https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-12-segmented-coverage
        const FIXED_SIZE: usize = 2 * u16::RAW_BYTE_LEN + 3 * u32::RAW_BYTE_LEN;
        const PER_SEGMENT_LEN: usize = 3 * u32::RAW_BYTE_LEN;

        (FIXED_SIZE + PER_SEGMENT_LEN * self.groups.len())
            .try_into()
            .unwrap()
    }
}

#[cfg(test)]
mod tests {
    use std::ops::RangeInclusive;

    use font_types::GlyphId;
    use read_fonts::{
        tables::cmap::{Cmap, CmapSubtable, PlatformId},
        FontData, FontRead,
    };

    use crate::{
        dump_table,
        tables::cmap::{
            self as write, CmapConflict, UNICODE_BMP_ENCODING, UNICODE_FULL_REPERTOIRE_ENCODING,
            WINDOWS_BMP_ENCODING, WINDOWS_FULL_REPERTOIRE_ENCODING,
        },
    };

    use super::{Cmap12, SequentialMapGroup};

    fn assert_generates_simple_cmap(mappings: Vec<(char, GlyphId)>) {
        let cmap = write::Cmap::from_mappings(mappings).unwrap();

        let bytes = dump_table(&cmap).unwrap();
        let font_data = FontData::new(&bytes);
        let cmap = Cmap::read(font_data).unwrap();

        assert_eq!(2, cmap.encoding_records().len(), "{cmap:?}");
        assert_eq!(
            vec![
                (PlatformId::Unicode, UNICODE_BMP_ENCODING),
                (PlatformId::Windows, WINDOWS_BMP_ENCODING)
            ],
            cmap.encoding_records()
                .iter()
                .map(|er| (er.platform_id(), er.encoding_id()))
                .collect::<Vec<_>>(),
            "{cmap:?}"
        );

        for encoding_record in cmap.encoding_records() {
            let CmapSubtable::Format4(cmap4) = encoding_record.subtable(font_data).unwrap() else {
                panic!("Expected a cmap4 in {encoding_record:?}");
            };

            // The spec example says entry_selector 4 but the calculation it gives seems to yield 2 (?)
            assert_eq!(
                (8, 8, 2, 0),
                (
                    cmap4.seg_count_x2(),
                    cmap4.search_range(),
                    cmap4.entry_selector(),
                    cmap4.range_shift()
                )
            );
            assert_eq!(cmap4.start_code(), &[10u16, 30u16, 153u16, 0xffffu16]);
            assert_eq!(cmap4.end_code(), &[20u16, 90u16, 480u16, 0xffffu16]);
            // The example starts at gid 1, we're starting at 0
            assert_eq!(cmap4.id_delta(), &[-10i16, -19i16, -81i16, 1i16]);
            assert_eq!(cmap4.id_range_offsets(), &[0u16, 0u16, 0u16, 0u16]);
        }
    }

    fn simple_cmap_mappings() -> Vec<(char, GlyphId)> {
        (10..=20)
            .chain(30..=90)
            .chain(153..=480)
            .enumerate()
            .map(|(idx, codepoint)| (char::from_u32(codepoint).unwrap(), GlyphId::new(idx as u32)))
            .collect()
    }

    // https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-4-segment-mapping-to-delta-values
    // "map characters 10-20, 30-90, and 153-480 onto a contiguous range of glyph indices"
    #[test]
    fn generate_simple_cmap4() {
        let mappings = simple_cmap_mappings();
        assert_generates_simple_cmap(mappings);
    }

    #[test]
    fn generate_cmap4_out_of_order_input() {
        let mut ordered = simple_cmap_mappings();
        let mut disordered = Vec::new();
        while !ordered.is_empty() {
            if ordered.len() % 2 == 0 {
                disordered.insert(0, ordered.remove(0));
            } else {
                disordered.push(ordered.remove(0));
            }
        }
        assert_ne!(ordered, disordered);
        assert_generates_simple_cmap(disordered);
    }

    #[test]
    fn generate_cmap4_large_values() {
        let mut mappings = simple_cmap_mappings();
        // Example from Texturina.
        let codepoint = char::from_u32(0xa78b).unwrap();
        let gid = GlyphId::new(153);
        mappings.push((codepoint, gid));

        let cmap = write::Cmap::from_mappings(mappings).unwrap();

        let bytes = dump_table(&cmap).unwrap();
        let font_data = FontData::new(&bytes);
        let cmap = Cmap::read(font_data).unwrap();
        assert_eq!(cmap.map_codepoint(codepoint), Some(gid));
    }

    #[test]
    fn bytes_are_reused() {
        // We emit extra encoding records assuming it's cheap. Make sure.
        let mappings = simple_cmap_mappings();
        let cmap_both = write::Cmap::from_mappings(mappings).unwrap();
        assert_eq!(2, cmap_both.encoding_records.len(), "{cmap_both:?}");

        let bytes_for_both = dump_table(&cmap_both).unwrap().len();

        for i in 0..cmap_both.encoding_records.len() {
            let mut cmap = cmap_both.clone();
            cmap.encoding_records.remove(i);
            let bytes_for_one = dump_table(&cmap).unwrap().len();
            assert_eq!(bytes_for_one + 8, bytes_for_both);
        }
    }

    fn non_bmp_cmap_mappings() -> Vec<(char, GlyphId)> {
        // contains four sequential map groups
        vec![
            // first group
            ('\u{1f12f}', GlyphId::new(481)),
            ('\u{1f130}', GlyphId::new(482)),
            // char 0x1f131 skipped, starts second group
            ('\u{1f132}', GlyphId::new(483)),
            ('\u{1f133}', GlyphId::new(484)),
            // gid 485 skipped, starts third group
            ('\u{1f134}', GlyphId::new(486)),
            // char 0x1f135 skipped, starts fourth group. identical duplicate bindings are fine
            ('\u{1f136}', GlyphId::new(488)),
            ('\u{1f136}', GlyphId::new(488)),
        ]
    }

    fn bmp_and_non_bmp_cmap_mappings() -> Vec<(char, GlyphId)> {
        let mut mappings = simple_cmap_mappings();
        mappings.extend(non_bmp_cmap_mappings());
        mappings
    }

    fn assert_cmap12_groups(
        font_data: FontData,
        cmap: &Cmap,
        record_index: usize,
        expected: &[(u32, u32, u32)],
    ) {
        let rec = &cmap.encoding_records()[record_index];
        let CmapSubtable::Format12(subtable) = rec.subtable(font_data).unwrap() else {
            panic!("Expected a cmap12 in {rec:?}");
        };
        let groups = subtable
            .groups()
            .iter()
            .map(|g| (g.start_char_code(), g.end_char_code(), g.start_glyph_id()))
            .collect::<Vec<_>>();
        assert_eq!(groups.len(), expected.len());
        assert_eq!(groups, expected);
    }

    #[test]
    fn generate_cmap4_and_12() {
        let mappings = bmp_and_non_bmp_cmap_mappings();

        let cmap = write::Cmap::from_mappings(mappings).unwrap();

        let bytes = dump_table(&cmap).unwrap();
        let font_data = FontData::new(&bytes);
        let cmap = Cmap::read(font_data).unwrap();

        assert_eq!(4, cmap.encoding_records().len(), "{cmap:?}");
        assert_eq!(
            vec![
                (PlatformId::Unicode, UNICODE_BMP_ENCODING),
                (PlatformId::Unicode, UNICODE_FULL_REPERTOIRE_ENCODING),
                (PlatformId::Windows, WINDOWS_BMP_ENCODING),
                (PlatformId::Windows, WINDOWS_FULL_REPERTOIRE_ENCODING)
            ],
            cmap.encoding_records()
                .iter()
                .map(|er| (er.platform_id(), er.encoding_id()))
                .collect::<Vec<_>>(),
            "{cmap:?}"
        );

        let encoding_records = cmap.encoding_records();
        let first_rec = &encoding_records[0];
        assert!(
            matches!(
                first_rec.subtable(font_data).unwrap(),
                CmapSubtable::Format4(_)
            ),
            "Expected a cmap4 in {first_rec:?}"
        );

        // (start_char_code, end_char_code, start_glyph_id)
        let expected_groups = vec![
            (10, 20, 0),
            (30, 90, 11),
            (153, 480, 72),
            (0x1f12f, 0x1f130, 481),
            (0x1f132, 0x1f133, 483),
            (0x1f134, 0x1f134, 486),
            (0x1f136, 0x1f136, 488),
        ];
        assert_cmap12_groups(font_data, &cmap, 1, &expected_groups);
        assert_cmap12_groups(font_data, &cmap, 3, &expected_groups);
    }

    #[test]
    fn generate_cmap12_only() {
        let mappings = non_bmp_cmap_mappings();

        let cmap = write::Cmap::from_mappings(mappings).unwrap();

        let bytes = dump_table(&cmap).unwrap();
        let font_data = FontData::new(&bytes);
        let cmap = Cmap::read(font_data).unwrap();

        assert_eq!(2, cmap.encoding_records().len(), "{cmap:?}");
        assert_eq!(
            vec![
                (PlatformId::Unicode, UNICODE_FULL_REPERTOIRE_ENCODING),
                (PlatformId::Windows, WINDOWS_FULL_REPERTOIRE_ENCODING)
            ],
            cmap.encoding_records()
                .iter()
                .map(|er| (er.platform_id(), er.encoding_id()))
                .collect::<Vec<_>>(),
            "{cmap:?}"
        );

        // (start_char_code, end_char_code, start_glyph_id)
        let expected_groups = vec![
            (0x1f12f, 0x1f130, 481),
            (0x1f132, 0x1f133, 483),
            (0x1f134, 0x1f134, 486),
            (0x1f136, 0x1f136, 488),
        ];
        assert_cmap12_groups(font_data, &cmap, 0, &expected_groups);
        assert_cmap12_groups(font_data, &cmap, 1, &expected_groups);
    }

    #[test]
    fn multiple_mappings_fails() {
        let mut mappings = non_bmp_cmap_mappings();
        // add an additional mapping to a different glyphId
        let (ch, gid1) = mappings[0];
        let gid2 = GlyphId::new(gid1.to_u32() + 1);
        mappings.push((ch, gid2));

        let result = write::Cmap::from_mappings(mappings);

        assert_eq!(result, Err(CmapConflict { ch, gid1, gid2 }))
    }

    struct MappingBuilder {
        mappings: Vec<(char, GlyphId)>,
        next_gid: u16,
    }

    impl Default for MappingBuilder {
        fn default() -> Self {
            Self {
                mappings: Default::default(),
                next_gid: 1,
            }
        }
    }

    impl MappingBuilder {
        fn extend(mut self, range: impl IntoIterator<Item = char>) -> Self {
            for c in range {
                let gid = GlyphId::new(self.next_gid as _);
                self.mappings.push((c, gid));
                self.next_gid += 1;
            }
            self
        }

        // compute the segments for the mapping
        fn compute(&mut self) -> Vec<RangeInclusive<char>> {
            self.mappings.sort();
            super::Format4SegmentComputer::new(&self.mappings)
                .compute()
                .into_iter()
                .map(|seg| self.mappings[seg.start_ix].0..=self.mappings[seg.end_ix].0)
                .collect()
        }

        fn build(mut self) -> Vec<(char, GlyphId)> {
            self.mappings.sort();
            self.mappings
        }
    }

    #[test]
    fn f4_segments_simple() {
        let mut one_big_discontiguous_mapping = MappingBuilder::default().extend(('a'..='z').rev());
        assert_eq!(one_big_discontiguous_mapping.compute(), ['a'..='z']);
    }

    #[test]
    fn f4_segments_combine_small() {
        let mut mapping = MappingBuilder::default()
            // backwards so gids are not contiguous
            .extend(['e', 'd', 'c', 'b', 'a'])
            // these two contiguous ranges aren't worth the cost, should merge
            // into the first and last respectively
            .extend('f'..='g')
            .extend('m'..='n')
            .extend(('o'..='z').rev());

        assert_eq!(mapping.compute(), ['a'..='g', 'm'..='z']);
    }

    #[test]
    fn f4_segments_keep() {
        let mut mapping = MappingBuilder::default()
            .extend('a'..='m')
            .extend(['o', 'n']);

        assert_eq!(mapping.compute(), ['a'..='m', 'n'..='o']);
    }

    fn expect_f4(mapping: &[(char, GlyphId)]) -> super::Cmap4 {
        let format4 = super::CmapSubtable::create_format_4(mapping).unwrap();
        let super::CmapSubtable::Format4(format4) = format4 else {
            panic!("O_o")
        };
        format4
    }

    // roundtrip the mapping from read-fonts
    fn get_read_mapping(table: &super::Cmap4) -> Vec<(char, GlyphId)> {
        let bytes = dump_table(table).unwrap();
        let readcmap = read_fonts::tables::cmap::Cmap4::read(bytes.as_slice().into()).unwrap();

        let mut mapping = readcmap
            .iter()
            .map(|(c, gid)| (char::from_u32(c).unwrap(), gid))
            .collect::<Vec<_>>();
        // cmap4 always ends with a 65535 => notdef entry. Sanity check
        // this and then pop it to avoid messing with tests
        assert_eq!(mapping.pop(), Some(('\u{FFFF}', GlyphId::NOTDEF)));
        mapping
    }

    #[test]
    fn f4_segment_len_one_uses_delta() {
        // if a segment is length one, we should always use the delta, since it's free.
        let mapping = MappingBuilder::default()
            .extend(['a', 'z', '1', '9'])
            .build();

        let format4 = expect_f4(&mapping);
        assert_eq!(format4.end_code.len(), 5); // 4 + 0xffff
        assert!(format4.glyph_id_array.is_empty());
        assert!(format4.id_delta.iter().all(|d| *d != 0));
    }

    #[test]
    fn f4_efficiency() {
        // one of these ranges should use id_delta, the other should use glyph id array
        let mapping = MappingBuilder::default()
            .extend('A'..='Z')
            .extend(('a'..='z').rev())
            .build();

        let format4 = expect_f4(&mapping);

        assert_eq!(
            format4.start_code,
            ['A' as u32 as u16, 'a' as u32 as u16, 0xffff]
        );

        assert_eq!(
            format4.end_code,
            ['Z' as u32 as u16, 'z' as u32 as u16, 0xffff]
        );

        assert_eq!(format4.id_delta, [-64, 0, 1]);
        assert_eq!(format4.id_range_offsets, [0, 4, 0]);

        let read_mapping = get_read_mapping(&format4);
        assert_eq!(mapping.len(), read_mapping.len());
        assert!(mapping == read_mapping);
    }

    #[test]
    fn f4_kinda_real_world() {
        // based on the first few hundred glyphs of oswald
        let mapping = MappingBuilder::default()
            .extend(['\r']) // CR
            .extend('\x20'..='\x7e') // ascii space to tilde
            .extend('\u{a0}'..='\u{ac}') // nbspace to logical not
            .extend('\u{ae}'..='\u{17f}') // registered to long s
            .extend(['\u{18f}', '\u{192}'])
            .build();

        let format4 = expect_f4(&mapping);
        // we added 3 ranges + 3 individual glyphs above, + the final 0xffff
        assert_eq!(format4.end_code.len(), 7);
        let read_mapping = get_read_mapping(&format4);

        assert_eq!(mapping.len(), read_mapping.len());
        assert!(mapping == read_mapping);
    }

    #[test]
    // a small ordered segment between two larger unordered segments;
    // merging this correctly requires us to consider the next segment as well
    fn f4_sandwich_segment() {
        let mapping = MappingBuilder::default()
            .extend(['\r'])
            .extend(('\x20'..='\x27').rev()) // cost = 8*2 + 8 = 24
            .extend('\x28'..='\x2c') // cost = 8
            .extend(('\x2d'..='\x34').rev()) // cost = 6*2 + 8 = 20
            // combined =
            // (8 + 5 + 6) * 2 + 8 = 46
            .extend('\x35'..='\x3e')
            .build();

        let format4 = expect_f4(&mapping);
        assert_eq!(format4.end_code.len(), 4);
    }

    // test that we correctly encode array lengths exceeding u16::MAX
    #[test]
    fn cmap12_length_calculation() {
        let more_than_16_bits = u16::MAX as u32 + 5;
        let groups = (0..more_than_16_bits)
            .map(|i| SequentialMapGroup::new(i, i, i))
            .collect();
        let cmap12 = Cmap12::new(0, groups);
        let bytes = crate::dump_table(&cmap12).unwrap();
        let read_it_back = Cmap12::read(bytes.as_slice().into()).unwrap();
        assert_eq!(read_it_back.groups.len() as u32, more_than_16_bits);
    }
}
