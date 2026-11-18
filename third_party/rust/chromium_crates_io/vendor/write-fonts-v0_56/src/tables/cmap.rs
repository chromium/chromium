//! the [cmap] table
//!
//! [cmap]: https://docs.microsoft.com/en-us/typography/opentype/spec/cmap

include!("../../generated/generated_cmap.rs");

use std::collections::HashMap;

use crate::search_range::SearchRange;

#[cfg(test)]
mod dmap_tests {
    use super::*;
    use crate::{dump_table, FontBuilder};
    use read_fonts::{FontRef, TableProvider};

    #[test]
    fn dmap_roundtrip() {
        let dmap = Dmap::new(vec![EncodingRecord::new(
            PlatformId::Windows,
            10,
            CmapSubtable::format_12(0, vec![SequentialMapGroup::new(65, 65, 70000)]),
        )]);
        let bytes = dump_table(&dmap).unwrap();
        assert_eq!(Dmap::read(FontData::new(&bytes)).unwrap(), dmap);
        let mut builder = FontBuilder::new();
        builder.add_table(&dmap).unwrap();
        let bytes = builder.build();
        let font = FontRef::new(&bytes).unwrap();
        assert_eq!(
            font.dmap().unwrap().as_cmap().map_codepoint('A'),
            Some(GlyphId::new(70000))
        );
        assert!(font.cmap().is_err());
    }
}

// https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#windows-platform-platform-id--3
const WINDOWS_BMP_ENCODING: u16 = 1;
const WINDOWS_FULL_REPERTOIRE_ENCODING: u16 = 10;

// https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#unicode-platform-platform-id--0
const UNICODE_BMP_ENCODING: u16 = 3;
const UNICODE_FULL_REPERTOIRE_ENCODING: u16 = 4;

/// The reason we can't build a format 4 subtable for a set of mappings.
#[derive(Clone, Copy, Debug, PartialEq, Eq)]
enum Format4Error {
    /// None of the chars are in the BMP.
    NoBmpMappings,
    /// A BMP char is mapped to a glyph id that doesn't fit in 16 bits.
    GlyphIdOverflow { ch: char, gid: GlyphId },
    /// The subtable's length, in bytes, doesn't fit in 16 bits.
    LengthOverflow(usize),
}

impl std::fmt::Display for Format4Error {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            Self::NoBmpMappings => write!(f, "there are no chars in the BMP"),
            Self::GlyphIdOverflow { ch, gid } => write!(
                f,
                "U+{:04X} is mapped to {gid}, which is above the format 4 maximum of 65535",
                *ch as u32
            ),
            Self::LengthOverflow(len) => write!(
                f,
                "length of {len} bytes is above the format 4 maximum of 65535"
            ),
        }
    }
}

impl CmapSubtable {
    /// Create a new format 4 subtable
    ///
    /// Returns an error if none of the input chars are in the BMP (i.e. have
    /// codepoints <= 0xFFFF), or if format 4 can't represent all of the BMP
    /// mappings.
    ///
    /// Invariants:
    ///
    /// - Inputs must be sorted and deduplicated.
    fn create_format_4(mappings: &[(char, GlyphId)]) -> Result<Self, Format4Error> {
        let mut end_code = Vec::with_capacity(mappings.len() + 1);
        let mut start_code = Vec::with_capacity(mappings.len() + 1);
        let mut id_deltas = Vec::with_capacity(mappings.len() + 1);
        let mut id_range_offsets = Vec::with_capacity(mappings.len() + 1);
        let mut glyph_ids = Vec::new();

        // format 4 only has 16-bit glyph ids
        if let Some((ch, gid)) = mappings
            .iter()
            .take_while(|(ch, _)| *ch <= '\u{FFFF}')
            .find(|(_, gid)| gid.to_u32() > 0xFFFF)
        {
            return Err(Format4Error::GlyphIdOverflow { ch: *ch, gid: *gid });
        }
        let segments = compute_format4_segments(mappings);
        if segments.is_empty() {
            return Err(Format4Error::NoBmpMappings);
        }
        let add_final_segment = segments.last().is_none_or(|seg| {
            (mappings[seg.start_ix].0, mappings[seg.end_ix].0) != ('\u{FFFF}', '\u{FFFF}')
        });
        let n_segments = segments.len() + add_final_segment as usize;
        let n_glyph_ids = segments
            .iter()
            .filter(|seg| seg.id_delta.is_none())
            .map(Format4Segment::len)
            .sum();
        Cmap4::checked_length(n_segments, n_glyph_ids)?;
        for (i, segment) in segments.into_iter().enumerate() {
            let start = mappings[segment.start_ix].0;
            let end = mappings[segment.end_ix].0;
            start_code.push(start as u32 as u16);
            end_code.push(end as u32 as u16);
            if let Some(delta) = segment.id_delta {
                // "The idDelta arithmetic is modulo 65536":
                id_deltas.push(delta as i16);
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
                // this is an offset within the subtable, so it fits in a u16
                // because the subtable's length does
                id_range_offsets.push(
                    id_range_offset
                        .try_into()
                        .expect("length checked before now"),
                );
                glyph_ids.extend(
                    mappings[segment.start_ix..=segment.end_ix]
                        .iter()
                        .map(|(_, gid)| u16::try_from(gid.to_u32()).expect("checked before now")),
                )
            }
        }

        // add the final segment:
        if add_final_segment {
            end_code.push(0xFFFF);
            start_code.push(0xFFFF);
            id_deltas.push(1);
            id_range_offsets.push(0);
        }

        Ok(Self::format_4(
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

        // we only create a format 12 subtable when there are mappings, so unwrap is safe
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
    /// The format 4 subtable is omitted if it can't represent every mapping
    /// in the BMP: that is, if it would be longer than 65535 bytes, or if a
    /// BMP char is mapped to a glyph id above 65535. In that case we log a
    /// warning, and the format 12 subtable contains all of the mappings, and
    /// is emitted even if every char is in the BMP. The spec [doesn't require] a format 4 alongside a
    /// format 12, and says that if both are present their BMP mappings should
    /// be identical, which rules out a partial format 4.
    ///
    /// Also see: <https://learn.microsoft.com/en-us/typography/opentype/spec/recom#cmap-table>
    ///
    /// [`cmap`]: https://learn.microsoft.com/en-us/typography/opentype/spec/cmap
    /// [format 4]: https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-4-segment-mapping-to-delta-values
    /// [format 12]: https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-12-segmented-coverage
    /// [doesn't require]: https://learn.microsoft.com/en-us/typography/opentype/spec/recom#cmap-table
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
        // we need to emit format 4 subtables, unless format 4 can't represent them
        let bmp_subtable = CmapSubtable::create_format_4(&mappings)
            .inspect_err(|e| log::warn!("omitting cmap format 4 subtable: {e}"))
            .ok();
        let has_format_4 = bmp_subtable.is_some();
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
        // emit format 12 subtables; and if there's no format 4 then format 12 has to
        // cover the BMP as well
        if !mappings.is_empty()
            && (!has_format_4 || mappings.iter().any(|(cp, _)| *cp > '\u{FFFF}'))
        {
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

// a segment of a format 4 subtable
#[derive(Clone, Copy, Debug)]
struct Format4Segment {
    // indices are into the source mappings
    start_ix: usize,
    end_ix: usize,
    id_delta: Option<i32>,
}

impl Format4Segment {
    fn len(&self) -> usize {
        self.end_ix - self.start_ix + 1
    }
}

/// Compute the segments for a format 4 subtable, using as few bytes as possible.
///
/// A segment maps a contiguous range of chars. It costs 8 bytes (end code,
/// start code, idDelta and idRangeOffset), and if its glyph ids aren't also
/// contiguous, it needs a glyphIdArray entry for each char, at 2 bytes each.
///
/// We split the mappings into runs of contiguous chars, then find the
/// cheapest segments for each run with [`push_cheapest_segments`]. So
/// segments never span a gap in the chars. A glyphIdArray segment could, by
/// mapping the missing chars to glyph 0, but we don't try that.
///
/// Chars above the BMP are ignored.
fn compute_format4_segments(mappings: &[(char, GlyphId)]) -> Vec<Format4Segment> {
    // ignore chars above BMP:
    let mappings = mappings
        .iter()
        .position(|(c, _)| u16::try_from(*c as u32).is_err())
        .map(|bad_idx| &mappings[..bad_idx])
        .unwrap_or(mappings);
    let mut segments = Vec::new();
    let mut run_start = 0;
    for run in mappings.chunk_by(|(c1, _), (c2, _)| *c1 as u32 + 1 == *c2 as u32) {
        push_cheapest_segments(run, run_start, &mut segments);
        run_start += run.len();
    }
    segments
}

/// Append the cheapest segments for `run`, whose chars are contiguous.
///
/// `offset` is the index of the start of `run` in the full list of mappings.
///
/// Let `best[k]` be the cost of the cheapest segments for `run[..k]`. The
/// last of those segments covers `run[j..k]` for some `j`, so `best[k]` is
/// the minimum over `j` of `best[j]` plus the cost of that segment. Trying
/// every `j` would be quadratic, but two observations make it linear:
///
/// - `best` never decreases, since dropping the last mapping from a set of
///   segments never makes them more expensive. So the cheapest idDelta
///   segment ending at `k` is the longest one, which starts where the
///   current run of contiguous glyph ids does.
/// - A glyphIdArray segment costs 2 bytes per mapping wherever it starts,
///   so the cheapest one ending at `k` starts at the `j` with the lowest
///   `best[j] - 2 * j`. We track that as we go.
fn push_cheapest_segments(
    run: &[(char, GlyphId)],
    offset: usize,
    segments: &mut Vec<Format4Segment>,
) {
    // a segment always costs 4 u16s (end, start, id_delta, id_range_offset)
    const SEGMENT_COST: usize = 4 * u16::RAW_BYTE_LEN;
    // and a glyphIdArray segment also costs a u16 for each char
    const GLYPH_ID_COST: usize = u16::RAW_BYTE_LEN;

    // best[k] is the cost of the cheapest segments for run[..k]
    let mut best = vec![0; run.len() + 1];
    // the start of the last of those segments, and whether it uses idDelta
    let mut last_segment = vec![(0, false); run.len() + 1];
    // the start of the current run of contiguous glyph ids
    let mut delta_start = 0;
    // the start of the cheapest glyphIdArray segment ending at k
    let mut array_start = 0;
    for k in 1..=run.len() {
        if k > 1 && run[k - 2].1.to_u32() + 1 != run[k - 1].1.to_u32() {
            delta_start = k - 1;
        }
        let delta_cost = best[delta_start] + SEGMENT_COST;
        let array_cost = best[array_start] + SEGMENT_COST + (k - array_start) * GLYPH_ID_COST;
        // on a tie, prefer idDelta
        (best[k], last_segment[k]) = if delta_cost <= array_cost {
            (delta_cost, (delta_start, true))
        } else {
            (array_cost, (array_start, false))
        };
        // best[k] - 2 * k < best[array_start] - 2 * array_start, without
        // going negative
        if best[k] < best[array_start] + (k - array_start) * GLYPH_ID_COST {
            array_start = k;
        }
    }

    // walk back from the end of the run to recover the segments
    let first_new = segments.len();
    let mut end = run.len();
    while end > 0 {
        let (start, use_delta) = last_segment[end];
        let (ch, gid) = run[start];
        segments.push(Format4Segment {
            start_ix: offset + start,
            end_ix: offset + end - 1,
            id_delta: use_delta.then(|| gid.to_u32() as i32 - ch as u32 as i32),
        });
        end = start;
    }
    segments[first_new..].reverse();
}

impl Cmap4 {
    /// Compute the length of a subtable with the given number of segments and glyph ids.
    ///
    /// Returns an error if the length doesn't fit in the 16-bit length field.
    fn checked_length(n_segments: usize, n_glyph_ids: usize) -> Result<u16, Format4Error> {
        // https://learn.microsoft.com/en-us/typography/opentype/spec/cmap#format-4-segment-mapping-to-delta-values
        // there are always 8 u16 fields
        const FIXED_SIZE: usize = 8 * u16::RAW_BYTE_LEN;
        const PER_SEGMENT_LEN: usize = 4 * u16::RAW_BYTE_LEN;

        let segment_len = n_segments * PER_SEGMENT_LEN;
        let gid_len = n_glyph_ids * u16::RAW_BYTE_LEN;

        let length = FIXED_SIZE + segment_len + gid_len;
        length
            .try_into()
            .map_err(|_| Format4Error::LengthOverflow(length))
    }

    fn compute_length(&self) -> u16 {
        // validation reports a length that doesn't fit
        Self::checked_length(self.end_code.len(), self.glyph_id_array.len())
            .expect("cmap4 overflow")
    }

    fn check_length(&self, ctx: &mut ValidationCtx) {
        if let Err(e) = Self::checked_length(self.end_code.len(), self.glyph_id_array.len()) {
            ctx.report(e);
        }
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

// We write the UVS tables inline, immediately after the record array, rather
// than as offset targets that the packer lays out.
//
// This is because of the 'length' field, which has to cover the UVS tables
// as well as the header: fonttools slices the subtable by it before
// decompiling, and ots requires it to be exact. write-fonts can't promise
// that for offset targets: the packer is free to reorder subtables, and
// identical subtables are deduplicated. In practice the default sort emits a
// table's children directly after it, and cmap's 32-bit offsets never trigger
// the fallback sort, so it would happen to work out; but writing the tables
// ourselves makes the layout (and so 'length') correct by construction.
macro_rules! impl_uvs_writer {
    ($cmap:ident, $format:literal) => {
        impl FontWrite for $cmap {
            fn write_into(&self, writer: &mut TableWriter) {
                ($format as u16).write_into(writer);
                self.compute_length().write_into(writer);
                u32::try_from(self.var_selector.len())
                    .unwrap()
                    .write_into(writer);

                // offsets are relative to the start of this subtable; the UVS tables
                // follow the record array, in record order, default before non-default
                let mut next_offset = Self::HEADER_LEN + self.var_selector.len() * Self::RECORD_LEN;
                let mut write_offset = |writer: &mut TableWriter, len: Option<usize>| match len {
                    Some(len) => {
                        (next_offset as u32).write_into(writer);
                        next_offset += len;
                    }
                    None => 0u32.write_into(writer),
                };
                for record in &self.var_selector {
                    record.var_selector.write_into(writer);
                    write_offset(
                        writer,
                        record.default_uvs.as_ref().map(DefaultUvs::compute_len),
                    );
                    write_offset(
                        writer,
                        record.non_default_uvs.as_ref().map(|uvs| uvs.compute_len()),
                    );
                }
                for record in &self.var_selector {
                    if let Some(uvs) = record.default_uvs.as_ref() {
                        uvs.write_into(writer);
                    }
                    if let Some(uvs) = record.non_default_uvs.as_ref() {
                        uvs.write_into(writer);
                    }
                }
            }

            fn table_type(&self) -> TableType {
                TableType::Named(stringify!($cmap))
            }
        }

        impl $cmap {
            // format, length, numVarSelectorRecords
            const HEADER_LEN: usize = u16::RAW_BYTE_LEN + 2 * u32::RAW_BYTE_LEN;
            // varSelector, defaultUVSOffset, nonDefaultUVSOffset
            const RECORD_LEN: usize = Uint24::RAW_BYTE_LEN + 2 * u32::RAW_BYTE_LEN;

            fn compute_length(&self) -> u32 {
                let uvs_len: usize = self
                    .var_selector
                    .iter()
                    .map(|record| {
                        record
                            .default_uvs
                            .as_ref()
                            .map(DefaultUvs::compute_len)
                            .unwrap_or(0)
                            + record
                                .non_default_uvs
                                .as_ref()
                                .map(|uvs| uvs.compute_len())
                                .unwrap_or(0)
                    })
                    .sum();

                (Self::HEADER_LEN + self.var_selector.len() * Self::RECORD_LEN + uvs_len)
                    .try_into()
                    .expect("UVS subtable overflow")
            }
        }
    };
}

impl_uvs_writer!(Cmap14, 14);
impl_uvs_writer!(Cmap15, 15);

impl DefaultUvs {
    fn compute_len(&self) -> usize {
        // numUnicodeValueRanges, then (startUnicodeValue, additionalCount) per range
        u32::RAW_BYTE_LEN + self.ranges.len() * (Uint24::RAW_BYTE_LEN + u8::RAW_BYTE_LEN)
    }
}

impl NonDefaultUvs {
    fn compute_len(&self) -> usize {
        // numUVSMappings, then (unicodeValue, glyphID) per mapping
        u32::RAW_BYTE_LEN + self.uvs_mapping.len() * (Uint24::RAW_BYTE_LEN + u16::RAW_BYTE_LEN)
    }
}

impl NonDefaultUvs24 {
    fn compute_len(&self) -> usize {
        u32::RAW_BYTE_LEN + self.uvs_mapping.len() * (2 * Uint24::RAW_BYTE_LEN)
    }
}

#[cfg(test)]
mod tests {
    use std::ops::RangeInclusive;

    use font_types::{GlyphId, Uint24};
    use read_fonts::{
        tables::cmap::{Cmap, CmapSubtable, PlatformId},
        FontData, FontRead, TableProvider,
    };

    use crate::{
        dump_table,
        from_obj::ToOwnedTable,
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

        assert_eq!(
            2,
            cmap.encoding_records().len(),
            "{:?}",
            cmap.encoding_records()
        );
        assert_eq!(
            vec![
                (PlatformId::Unicode, UNICODE_BMP_ENCODING),
                (PlatformId::Windows, WINDOWS_BMP_ENCODING)
            ],
            cmap.encoding_records()
                .iter()
                .map(|er| (er.platform_id(), er.encoding_id()))
                .collect::<Vec<_>>()
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
    fn generate_cmap4_large_positive_delta() {
        let mut mappings = simple_cmap_mappings();
        // Example from Cactus Classical Serif: a low codepoint mapped to a
        // glyph more than 32767 ids above it.
        let codepoint = '\u{2026}';
        let gid = GlyphId::new(58562);
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

        assert_eq!(
            4,
            cmap.encoding_records().len(),
            "{:?}",
            cmap.encoding_records()
        );
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
                .collect::<Vec<_>>()
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

        assert_eq!(
            2,
            cmap.encoding_records().len(),
            "{:?}",
            cmap.encoding_records()
        );
        assert_eq!(
            vec![
                (PlatformId::Unicode, UNICODE_FULL_REPERTOIRE_ENCODING),
                (PlatformId::Windows, WINDOWS_FULL_REPERTOIRE_ENCODING)
            ],
            cmap.encoding_records()
                .iter()
                .map(|er| (er.platform_id(), er.encoding_id()))
                .collect::<Vec<_>>()
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
            super::compute_format4_segments(&self.mappings)
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

    #[test]
    // ASCII from Adamina. In 0x3A..=0x7E the longest run of contiguous glyph
    // ids is 5, which costs more to split out than it saves. Merging any two
    // neighbouring segments doesn't help either, so a greedy merge (which we
    // used to do) leaves 9 segments where one is cheapest.
    fn f4_segments_adamina() {
        #[rustfmt::skip]
        let gids = [
            // space to slash
            175, 144, 150, 146, 180, 196, 198, 151, 159, 160, 138, 185, 143, 163, 147, 153,
            // digits
            120, 121, 122, 123, 124, 125, 126, 127, 128, 129,
            // colon to at
            142, 152, 191, 189, 190, 148, 197,
            // A to Z
            1, 9, 10, 12, 14, 19, 20, 21, 22, 27, 28, 29, 30, 31, 33, 41, 43, 44, 45, 46, 47,
            52, 53, 54, 55, 57,
            // left bracket to grave
            157, 139, 158, 206, 154, 212,
            // a to z
            58, 66, 67, 69, 71, 76, 77, 78, 79, 85, 86, 87, 88, 89, 91, 99, 101, 102, 103, 105,
            106, 111, 112, 113, 114, 117,
            // left brace to tilde
            155, 204, 156, 193,
        ];
        let mappings = ('\x20'..='\x7e').zip(gids.map(GlyphId::new)).collect();
        let mut mapping = MappingBuilder {
            mappings,
            ..Default::default()
        };

        assert_eq!(
            mapping.compute(),
            ['\x20'..='\x2f', '\x30'..='\x39', '\x3a'..='\x7e']
        );
    }

    /// The cost of the cheapest segments for `mappings`, trying every split.
    ///
    /// This excludes the header and the final segment.
    fn cheapest_segments_cost(mappings: &[(char, GlyphId)]) -> usize {
        mappings
            .chunk_by(|(c1, _), (c2, _)| *c1 as u32 + 1 == *c2 as u32)
            .map(|run| {
                // best[k] is the cost of the cheapest segments for run[..k]
                let mut best = vec![0; run.len() + 1];
                for end in 1..=run.len() {
                    let cheapest = (0..end)
                        .map(|start| {
                            let segment = &run[start..end];
                            let use_delta = segment
                                .windows(2)
                                .all(|pair| pair[0].1.to_u32() + 1 == pair[1].1.to_u32());
                            let glyph_ids_cost = if use_delta { 0 } else { 2 * segment.len() };
                            best[start] + 8 + glyph_ids_cost
                        })
                        .min()
                        .unwrap();
                    best[end] = cheapest;
                }
                best[run.len()]
            })
            .sum()
    }

    #[test]
    fn f4_segments_are_cheapest() {
        use rand::{rngs::StdRng, RngExt, SeedableRng};

        // the header and the final segment
        const FIXED_COST: usize = 16 + 8;

        let mut rng = StdRng::seed_from_u64(4);
        for _ in 0..500 {
            // how often each glyph id follows on from the previous one
            let p_contiguous = rng.random_range(0.0..1.0);
            let mut mappings = Vec::new();
            let (mut ch, mut gid) = (0x20, 1);
            for _ in 0..rng.random_range(1..40) {
                mappings.push((char::from_u32(ch).unwrap(), GlyphId::new(gid)));
                ch += if rng.random_bool(0.05) { 2 } else { 1 };
                gid = if rng.random_bool(p_contiguous) {
                    gid + 1
                } else {
                    rng.random_range(1..100)
                };
            }

            let format4 = expect_f4(&mappings);
            assert_eq!(get_read_mapping(&format4), mappings);
            assert_eq!(
                dump_table(&format4).unwrap().len(),
                FIXED_COST + cheapest_segments_cost(&mappings),
                "{mappings:?}"
            );
        }
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

    #[test]
    fn cmap14_round_trip() {
        let font = read_fonts::FontRef::new(font_test_data::CMAP14_FONT1).unwrap();
        let cmap = font.cmap().unwrap();
        let cmap14 = cmap
            .encoding_records()
            .iter()
            .find_map(|record| match record.subtable(cmap.offset_data()) {
                Ok(read_fonts::tables::cmap::CmapSubtable::Format14(table)) => Some(table),
                _ => None,
            })
            .unwrap();
        let owned: write::Cmap14 = cmap14.to_owned_table();
        let bytes = dump_table(&owned).unwrap();
        assert_eq!(
            bytes,
            &cmap14.offset_data().as_bytes()[..cmap14.length() as usize]
        );
    }

    #[test]
    fn cmap15_round_trip() {
        use font_test_data::cmap::format15;
        let bytes = format15(0xe0100, &[65], &[(66, 65536), (67, 70000), (68, 0xffffff)]);
        let read = read_fonts::tables::cmap::Cmap15::read(bytes.as_slice().into()).unwrap();
        let owned: write::Cmap15 = read.to_owned_table();
        assert_eq!(dump_table(&owned).unwrap(), bytes);
        let cmap = write::Cmap::new(vec![write::EncodingRecord::new(
            PlatformId::Unicode,
            5,
            owned.into(),
        )]);
        let bytes = dump_table(&cmap).unwrap();
        let dmap = write::Dmap::new(cmap.encoding_records.clone());
        assert_eq!(dump_table(&dmap).unwrap(), bytes);
        let cmap = Cmap::read(bytes.as_slice().into()).unwrap();
        let (_, subtable) = cmap.uvs_subtable().unwrap();
        assert_eq!(
            subtable.map_variant(67u32, 0xe0100u32),
            Some(read_fonts::tables::cmap::MapVariant::Variant(GlyphId::new(
                70000
            )))
        );
    }

    #[test]
    fn cmap15_offsets_and_record_widths() {
        let non_default = write::NonDefaultUvs24::new(vec![write::UvsMapping24::new(
            Uint24::new(0x4e08),
            Uint24::new(70000),
        )]);
        let cmap = write::Cmap15::new(vec![
            write::VariationSelector15::new(
                Uint24::new(0xe0100),
                Some(default_uvs()),
                Some(non_default.clone()),
            ),
            write::VariationSelector15::new(
                Uint24::new(0xe0101),
                Some(default_uvs()),
                Some(non_default.clone()),
            ),
            write::VariationSelector15::new(Uint24::new(0xe0102), None, Some(non_default)),
            write::VariationSelector15::new(Uint24::new(0xe0103), Some(default_uvs()), None),
        ]);
        let bytes = dump_table(&cmap).unwrap();
        let read = read_fonts::tables::cmap::Cmap15::read(bytes.as_slice().into()).unwrap();
        // 10-byte header + four 11-byte records, then 8-byte default and
        // 10-byte non-default UVS tables. Identical targets are not shared.
        let offsets: Vec<_> = read
            .var_selector()
            .iter()
            .flat_map(|record| {
                [
                    record.default_uvs_offset().offset().to_u32(),
                    record.non_default_uvs_offset().offset().to_u32(),
                ]
            })
            .collect();
        assert_eq!(offsets, [54, 62, 72, 80, 0, 90, 100, 0]);
        assert_eq!(read.length(), 108);
        assert_eq!(bytes.len(), 108);
        for selector in 0xe0100..=0xe0102u32 {
            assert_eq!(
                read_fonts::tables::cmap::UvsSubtable::from(read.clone())
                    .map_variant(0x4e08u32, selector),
                Some(read_fonts::tables::cmap::MapVariant::Variant(GlyphId::new(
                    70000
                )))
            );
        }
        let owned: write::Cmap15 = read.to_owned_table();
        assert_eq!(dump_table(&owned).unwrap(), bytes);
    }

    fn default_uvs() -> write::DefaultUvs {
        write::DefaultUvs::new(1, vec![write::UnicodeRange::new(Uint24::new(0x4E00), 2)])
    }

    fn non_default_uvs() -> write::NonDefaultUvs {
        write::NonDefaultUvs::new(1, vec![write::UvsMapping::new(Uint24::new(0x4E08), 25)])
    }

    // returns the bytes and the offsets written in each record, checking that
    // the written 'length' matches the bytes
    fn dump_cmap14(cmap14: &write::Cmap14) -> (Vec<u8>, Vec<u32>) {
        let bytes = dump_table(cmap14).unwrap();
        let read = read_fonts::tables::cmap::Cmap14::read(bytes.as_slice().into()).unwrap();
        assert_eq!(read.length() as usize, bytes.len());
        let offsets = read
            .var_selector()
            .iter()
            .flat_map(|rec| [rec.default_uvs_offset(), rec.non_default_uvs_offset()])
            .map(|off| off.offset().to_u32())
            .collect();
        (bytes, offsets)
    }

    // if we wrote the UVS tables as offset targets, the packer would dedup these
    #[test]
    fn cmap14_identical_uvs_tables_are_not_shared() {
        let cmap14 = write::Cmap14::new(vec![
            write::VariationSelector::new(
                Uint24::new(0xE0100),
                Some(default_uvs()),
                Some(non_default_uvs()),
            ),
            write::VariationSelector::new(
                Uint24::new(0xE0101),
                Some(default_uvs()),
                Some(non_default_uvs()),
            ),
        ]);
        let (bytes, offsets) = dump_cmap14(&cmap14);
        // 10 byte header + 2 * 11 byte records = 32, then a default table (8)
        // and a non-default table (9) per record
        assert_eq!(offsets, [32, 40, 49, 57]);
        assert_eq!(bytes.len(), 66);
    }

    #[test]
    fn cmap14_null_offsets() {
        let cmap14 = write::Cmap14::new(vec![
            write::VariationSelector::new(Uint24::new(0xE0100), None, Some(non_default_uvs())),
            write::VariationSelector::new(Uint24::new(0xE0101), Some(default_uvs()), None),
        ]);
        let (bytes, offsets) = dump_cmap14(&cmap14);
        // missing tables take no space, and the tables are written in record order
        assert_eq!(offsets, [0, 32, 41, 0]);
        let tail = [
            dump_table(&non_default_uvs()).unwrap(),
            dump_table(&default_uvs()).unwrap(),
        ]
        .concat();
        assert_eq!(&bytes[32..], tail);
    }

    fn cmap4_has_a_unique_final_segment<I>(mappings: I)
    where
        I: IntoIterator<Item = (char, GlyphId)>,
    {
        let cmap = crate::tables::cmap::Cmap::from_mappings(mappings).unwrap();
        for record in &cmap.encoding_records {
            let crate::tables::cmap::CmapSubtable::Format4(cmap4) = &*record.subtable else {
                continue;
            };
            if !matches!((cmap4.start_code.as_slice(), cmap4.end_code.as_slice()),
                (&[.., before_last_start, 0xFFFF], &[.., before_last_end, 0xFFFF])
                    if before_last_start != 0xFFFF || before_last_end != 0xFFFF)
            {
                panic!(
                    "Expected cmap4 to end with a single (0xFFFF, 0xFFFF) segment, but found {:?}",
                    (&cmap4.start_code, &cmap4.end_code)
                );
            }
        }
    }

    #[test]
    fn cmap4_final_segment_is_not_duplicated() {
        cmap4_has_a_unique_final_segment([('a', GlyphId::new(1)), ('\u{FFFF}', GlyphId::new(0))]);
    }

    #[test]
    fn cmap4_final_segment_is_added_even_if_last_generated_segment_ends_with_0xffff() {
        cmap4_has_a_unique_final_segment([
            ('a', GlyphId::new(1)),
            ('\u{FFFE}', GlyphId::new(0)),
            ('\u{FFFF}', GlyphId::new(0)),
        ]);
    }

    type RecordKey = (PlatformId, u16, u16);

    // write the cmap and read it back, returning the (platform, encoding, format)
    // of each encoding record along with the mappings in its subtable
    fn read_back(cmap: &write::Cmap) -> Vec<(RecordKey, Vec<(char, GlyphId)>)> {
        let bytes = dump_table(cmap).unwrap();
        let font_data = FontData::new(&bytes);
        let cmap = Cmap::read(font_data).unwrap();
        cmap.encoding_records()
            .iter()
            .map(|rec| {
                let subtable = rec.subtable(font_data).unwrap();
                let mut mappings = subtable
                    .iter()
                    .map(|(c, gid)| (char::from_u32(c).unwrap(), gid))
                    .collect::<Vec<_>>();
                if subtable.format() == 4 {
                    // cmap4 always ends with a 65535 => notdef entry
                    assert_eq!(mappings.pop(), Some(('\u{FFFF}', GlyphId::NOTDEF)));
                }
                let key = (rec.platform_id(), rec.encoding_id(), subtable.format());
                (key, mappings)
            })
            .collect()
    }

    // assert that we emit only format 12, containing all the mappings
    fn assert_format_12_only(mut mappings: Vec<(char, GlyphId)>) {
        let cmap = write::Cmap::from_mappings(mappings.clone()).unwrap();
        mappings.sort();
        mappings.dedup();
        assert_eq!(
            read_back(&cmap),
            [
                (
                    (PlatformId::Unicode, UNICODE_FULL_REPERTOIRE_ENCODING, 12),
                    mappings.clone()
                ),
                (
                    (PlatformId::Windows, WINDOWS_FULL_REPERTOIRE_ENCODING, 12),
                    mappings
                ),
            ]
        );
    }

    // one contiguous run of BMP chars with reversed glyph ids, which has to
    // be stored in the glyph id array, and is too long for format 4
    fn too_long_for_format_4() -> MappingBuilder {
        MappingBuilder::default().extend(('\u{1000}'..='\u{AC3F}').rev())
    }

    #[test]
    fn omit_format_4_if_too_long() {
        let mappings = too_long_for_format_4().build();
        assert_eq!(mappings.len(), 40_000);
        assert_eq!(
            super::CmapSubtable::create_format_4(&mappings),
            // two segments, and a glyph id for each mapping
            Err(super::Format4Error::LengthOverflow(16 + 2 * 8 + 40_000 * 2))
        );
        assert_format_12_only(mappings);
    }

    #[test]
    fn omit_format_4_if_too_long_with_supplementary_chars() {
        let mappings = too_long_for_format_4()
            .extend(['\u{1F600}', '\u{1F602}', '\u{20000}'])
            .build();
        assert_format_12_only(mappings);
    }

    #[test]
    fn omit_format_4_if_bmp_glyph_id_too_large() {
        let mut mappings = simple_cmap_mappings();
        let big_gid = ('\u{4E00}', GlyphId::new(70_000));
        mappings.push(big_gid);
        mappings.sort();
        assert_eq!(
            super::CmapSubtable::create_format_4(&mappings),
            Err(super::Format4Error::GlyphIdOverflow {
                ch: big_gid.0,
                gid: big_gid.1
            })
        );
        assert_format_12_only(mappings);
    }

    #[test]
    fn empty_mappings() {
        let cmap = write::Cmap::from_mappings([]).unwrap();
        assert!(read_back(&cmap).is_empty());
    }

    #[test]
    fn large_supplementary_glyph_id_keeps_format_4() {
        let mut mappings = bmp_and_non_bmp_cmap_mappings();
        mappings.push(('\u{20000}', GlyphId::new(70_000)));
        mappings.sort();
        mappings.dedup();
        let bmp_mappings = simple_cmap_mappings();

        let cmap = write::Cmap::from_mappings(mappings.clone()).unwrap();
        assert_eq!(
            read_back(&cmap),
            [
                (
                    (PlatformId::Unicode, UNICODE_BMP_ENCODING, 4),
                    bmp_mappings.clone()
                ),
                (
                    (PlatformId::Unicode, UNICODE_FULL_REPERTOIRE_ENCODING, 12),
                    mappings.clone()
                ),
                ((PlatformId::Windows, WINDOWS_BMP_ENCODING, 4), bmp_mappings),
                (
                    (PlatformId::Windows, WINDOWS_FULL_REPERTOIRE_ENCODING, 12),
                    mappings
                ),
            ]
        );
    }

    #[test]
    fn oversized_cmap4_fails_validation() {
        // a segment covering U+0000..=U+7FFF using the glyph id array, then
        // the final segment
        let cmap4 = write::Cmap4::new(
            0,
            vec![0x7FFF, 0xFFFF],
            vec![0, 0xFFFF],
            vec![0, 1],
            vec![4, 0],
            (1..=0x8000).collect(),
        );
        assert!(matches!(
            dump_table(&cmap4),
            Err(crate::error::Error::ValidationFailed(_))
        ));

        let cmap = write::Cmap::new(vec![write::EncodingRecord::new(
            PlatformId::Windows,
            WINDOWS_BMP_ENCODING,
            cmap4.into(),
        )]);
        assert!(matches!(
            dump_table(&cmap),
            Err(crate::error::Error::ValidationFailed(_))
        ));
    }
}
