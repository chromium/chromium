//! impl subset() for cmap table
use std::cmp::Ordering;

use crate::{
    serialize::{ObjIdx, OffsetWhence, SerializeErrorFlags, Serializer},
    Plan, Subset,
    SubsetError::{self, SubsetTableError},
};

use crate::FastHashMap;
use read_fonts::{
    collections::IntSet,
    tables::cmap::{
        Cmap, Cmap12, Cmap13, Cmap14, Cmap15, Cmap4, CmapSubtable, DefaultUvs, Dmap,
        EncodingRecord, NonDefaultUvs, NonDefaultUvs24, PlatformId, SequentialMapGroup,
        VariationSelector, VariationSelector15,
    },
    types::{FixedSize, GlyphId},
    types::{Offset32, Uint24},
    FontRef, TopLevelTable,
};
use skrifa::raw::tables::cmap::UnicodeRange;
use write_fonts::FontBuilder;

const INVALID_UNICODE_CHAR: u32 = u32::MAX;
const UNICODE_MAX: u32 = 0x10FFFF_u32;
const HEADER_SIZE: usize = 4;
// reference: subset() for cmap table in harfbuzz
// <https://github.com/harfbuzz/harfbuzz/blob/b14def8bb32f32c32f2e2e9e1ce3efef2a242ca0/src/hb-ot-cmap-table.hh#L1920>
impl Subset for Cmap<'_> {
    fn subset(
        &self,
        plan: &Plan,
        font: &FontRef,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        let retained_encoding_records: Vec<(usize, &EncodingRecord)> = self
            .encoding_records()
            .iter()
            .enumerate()
            .filter(|(_, r)| retain_encoding_record_for_subset(r, self))
            .collect();

        let mut has_unicode_bmp = false;
        let mut has_unicode_usc4 = false;
        let mut has_ms_bmp = false;
        let mut has_ms_usc4 = false;
        let mut has_format12 = false;

        for (_, record) in retained_encoding_records.iter() {
            if record
                .subtable(self.offset_data())
                .is_ok_and(|t| t.format() == 12)
            {
                has_format12 = true;
            }

            if record.platform_id() == PlatformId::Unicode && record.encoding_id() == 3 {
                has_unicode_bmp = true;
            } else if record.platform_id() == PlatformId::Unicode && record.encoding_id() == 4 {
                has_unicode_usc4 = true;
            } else if record.platform_id() == PlatformId::Windows && record.encoding_id() == 1 {
                has_ms_bmp = true;
            } else if record.platform_id() == PlatformId::Windows && record.encoding_id() == 10 {
                has_ms_usc4 = true;
            }
        }

        if !has_format12 && !has_unicode_bmp && !has_ms_bmp {
            return Err(SubsetTableError(Cmap::TAG));
        }

        if has_format12 && (!has_unicode_usc4 && !has_ms_usc4) {
            return Err(SubsetTableError(Cmap::TAG));
        }

        let has_dmap = font
            .table_data(read_fonts::types::Tag::new(b"DMAP"))
            .is_some();
        serialize_cmap(self, s, plan, &retained_encoding_records, has_dmap)
            .map_err(|_| SubsetTableError(Cmap::TAG))
    }
}

impl Subset for Dmap<'_> {
    fn subset(
        &self,
        plan: &Plan,
        _font: &FontRef,
        s: &mut Serializer,
        _builder: &mut FontBuilder,
    ) -> Result<(), SubsetError> {
        serialize_dmap(&self.as_cmap(), s, plan).map_err(|error| {
            if error != SerializeErrorFlags::SERIALIZE_ERROR_EMPTY {
                s.set_err(error);
            }
            SubsetTableError(Dmap::TAG)
        })
    }
}

fn serialize_dmap(dmap: &Cmap, s: &mut Serializer, plan: &Plan) -> Result<(), SerializeErrorFlags> {
    if dmap.encoding_records().len() != dmap.num_tables() as usize {
        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
    }
    s.embed(0u16)?;
    let count_pos = s.embed(0u16)?;
    let mut count = 0;
    let variation_index = dmap.uvs_subtable().map(|(index, _)| index as usize);
    for (index, record) in dmap.encoding_records().iter().enumerate() {
        let retain = retain_encoding_record_for_subset(record, dmap)
            || (record.platform_id() == PlatformId::Unicode
                && matches!(record.encoding_id(), 5 | 6));
        if !retain {
            continue;
        }
        let subtable = record
            .subtable(dmap.offset_data())
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR))?;
        if !matches!(subtable.format(), 4 | 12 | 13 | 14 | 15) {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
        }
        if matches!(subtable.format(), 14 | 15) && Some(index) != variation_index {
            continue;
        }
        let truncated = match &subtable {
            CmapSubtable::Format12(table) => table.groups().len() != table.num_groups() as usize,
            CmapSubtable::Format13(table) => table.groups().len() != table.num_groups() as usize,
            _ => false,
        };
        if truncated {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
        }
        // Project the effective mapping onto each subtable's nonzero coverage,
        // as for cmap. If pruning changes which nominal subtable is selected,
        // a lower-priority subtable must not introduce different glyphs.
        // Zero nominal mappings can be omitted because they fall back to cmap.
        let mappings: Vec<_> = plan
            .unicode_to_new_gid_list
            .iter()
            .filter_map(|(cp, gid)| {
                let old_gid = subtable.map_codepoint(*cp)?;
                (old_gid != GlyphId::NOTDEF).then_some((*cp, *gid))
            })
            .collect();
        if subtable.format() == 4
            && mappings
                .iter()
                .any(|(_, gid)| gid.to_u32() > u16::MAX as u32)
        {
            return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW));
        }
        if serialize_encoding_record(record, &subtable, s, &mappings, plan, None, true)?.is_some() {
            count += 1;
        }
    }
    if count == 0 {
        return Err(SerializeErrorFlags::SERIALIZE_ERROR_EMPTY);
    }
    s.check_assign::<u16>(
        count_pos,
        count,
        SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
    )?;
    Ok(())
}

fn retain_encoding_record_for_subset(record: &EncodingRecord, cmap: &Cmap) -> bool {
    (record.platform_id() == PlatformId::Unicode && record.encoding_id() == 3)
        || (record.platform_id() == PlatformId::Unicode && record.encoding_id() == 4)
        || (record.platform_id() == PlatformId::Windows && record.encoding_id() == 1)
        || (record.platform_id() == PlatformId::Windows && record.encoding_id() == 10)
        || record
            .subtable(cmap.offset_data())
            .is_ok_and(|t| matches!(t.format(), 14 | 15))
}

fn can_drop_format12(
    cmap12_record: &EncodingRecord,
    cmap12_subset_unicodes: &IntSet<u32>,
    cmap: &Cmap,
    retained_encoding_records: &[(usize, &EncodingRecord)],
    unicodes_cache: &mut SubtableUnicodeCache,
    subset_unicodes: &IntSet<u32>,
    num_glyphs: usize,
) -> bool {
    for cp in cmap12_subset_unicodes.iter() {
        if cp >= 0x10000 {
            return false;
        }
    }

    let (target_platform, target_encoding) =
        if cmap12_record.platform_id() == PlatformId::Unicode && cmap12_record.encoding_id() == 4 {
            (PlatformId::Unicode, 3_u16)
        } else if cmap12_record.platform_id() == PlatformId::Windows
            && cmap12_record.encoding_id() == 10
        {
            (PlatformId::Windows, 1)
        } else {
            return false;
        };

    let target_language = cmap12_record
        .subtable(cmap.offset_data())
        .unwrap()
        .language();

    for (rec_idx, rec) in retained_encoding_records.iter() {
        let Ok(subtable) = rec.subtable(cmap.offset_data()) else {
            continue;
        };
        if rec.platform_id() != target_platform
            || rec.encoding_id() != target_encoding
            || subtable.language() != target_language
        {
            continue;
        }

        let Some(sibling_unicodes) = unicodes_cache.set_for(*rec_idx, &subtable, num_glyphs) else {
            continue;
        };

        return cmap12_subset_unicodes.iter().cmp(
            subset_unicodes
                .iter()
                .filter(|v| sibling_unicodes.contains(*v)),
        ) == Ordering::Equal;
    }
    false
}

/// The mappings a cmap subtable keeps, for the codepoints `keep` accepts.
///
/// Without DMAP these are the font's best mappings projected onto the
/// subtable, as HarfBuzz does. With DMAP, whose mappings take precedence in
/// the best list, each subtable maps through its own original glyphs.
fn cmap_mappings(
    subtable: &CmapSubtable,
    plan: &Plan,
    has_dmap: bool,
    keep: impl Fn(u32) -> bool,
) -> Vec<(u32, GlyphId)> {
    let mappings = plan
        .unicode_to_new_gid_list
        .iter()
        .filter(|(cp, _)| keep(*cp));
    if !has_dmap {
        return mappings.copied().collect();
    }
    mappings
        .filter_map(|&(cp, _)| {
            let old_gid = subtable.map_codepoint(cp)?;
            Some((cp, *plan.glyph_map.get(&old_gid)?))
        })
        .collect()
}

fn serialize_cmap(
    cmap: &Cmap,
    s: &mut Serializer,
    plan: &Plan,
    retained_encoding_records: &[(usize, &EncodingRecord)],
    has_dmap: bool,
) -> Result<(), SerializeErrorFlags> {
    // allocate header: version + numTables
    s.allocate_size(HEADER_SIZE, false)?;

    let mut format4_objidx = None;
    let mut format12_objidx = None;
    let mut format14_objidx = None;
    let mut format15_objidx = None;
    //TODO: add support for cmap_cache in plan accelerator
    let mut unicodes_cache =
        SubtableUnicodeCache::new(cmap.offset_data().as_bytes().as_ptr() as usize);
    for (rec_idx, record) in retained_encoding_records.iter() {
        if s.in_error() {
            return Err(s.error());
        }

        let Ok(subtable) = record.subtable(cmap.offset_data()) else {
            continue;
        };

        let format = subtable.format();
        if !matches!(format, 4 | 12 | 14 | 15) {
            continue;
        }

        if format == 4 {
            let Some(unicodes_set) =
                unicodes_cache.set_for(*rec_idx, &subtable, plan.font_num_glyphs)
            else {
                continue;
            };
            let cp_to_new_gid_list =
                cmap_mappings(&subtable, plan, has_dmap, |cp| unicodes_set.contains(cp));

            let snap = s.snapshot();
            format4_objidx = match serialize_encoding_record(
                record,
                &subtable,
                s,
                &cp_to_new_gid_list,
                plan,
                format4_objidx,
                false,
            ) {
                Ok(obj_idx) => obj_idx,
                Err(e) => {
                    if s.in_error() && s.only_overflow() {
                        // cmap4 overflowed, drop cmap4 table and continue serialization with other tables
                        s.revert_snapshot(snap);
                        None
                    } else {
                        return Err(e);
                    }
                }
            }
        } else if format == 12 {
            let Some(unicodes_set) =
                unicodes_cache.set_for(*rec_idx, &subtable, plan.font_num_glyphs)
            else {
                continue;
            };
            let cmap12_subset_unicodes =
                IntSet::from_iter(plan.unicodes.iter().filter(|v| unicodes_set.contains(*v)));

            if can_drop_format12(
                record,
                &cmap12_subset_unicodes,
                cmap,
                retained_encoding_records,
                &mut unicodes_cache,
                &plan.unicodes,
                plan.font_num_glyphs,
            ) {
                continue;
            }

            let cp_to_new_gid_list = cmap_mappings(&subtable, plan, has_dmap, |cp| {
                cmap12_subset_unicodes.contains(cp)
            });
            format12_objidx = serialize_encoding_record(
                record,
                &subtable,
                s,
                &cp_to_new_gid_list,
                plan,
                format12_objidx,
                false,
            )?;
        } else if format == 14 || format == 15 {
            let objidx = if format == 14 {
                &mut format14_objidx
            } else {
                &mut format15_objidx
            };
            *objidx = serialize_encoding_record(
                record,
                &subtable,
                s,
                &plan.unicode_to_new_gid_list,
                plan,
                *objidx,
                false,
            )?;
        }
    }

    let num_retained_records = (s.length() - HEADER_SIZE) / EncodingRecord::RAW_BYTE_LEN;
    s.check_assign::<u16>(
        2,
        num_retained_records,
        SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
    )?;

    // Fail if format 4 was dropped and there is no cmap12.
    if format4_objidx.is_none() && format12_objidx.is_none() {
        return Err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER);
    }

    Ok(())
}

fn serialize_encoding_record(
    record: &EncodingRecord,
    cmap_subtable: &CmapSubtable,
    s: &mut Serializer,
    cp_to_new_gid_list: &[(u32, GlyphId)],
    plan: &Plan,
    obj_idx: Option<ObjIdx>,
    dmap: bool,
) -> Result<Option<ObjIdx>, SerializeErrorFlags> {
    let snap = s.snapshot();
    s.embed(record.platform_id())?;
    s.embed(record.encoding_id())?;
    let offset_pos = s.embed(Offset32::new(0))?;

    let subtable_idx = if let Some(idx) = obj_idx {
        idx
    } else {
        s.push()?;
        let init_len = s.length();
        let init_head = s.head();
        match cmap_subtable.serialize(s, plan, cp_to_new_gid_list) {
            Ok(()) => {
                if s.length() > init_len {
                    if dmap {
                        match cmap_subtable.format() {
                            4 => s.copy_assign(init_head + 4, 0u16),
                            12 | 13 => s.copy_assign(init_head + 8, 0u32),
                            _ => (),
                        }
                    }
                    s.pop_pack(true).ok_or_else(|| s.error())?
                } else {
                    s.pop_discard();
                    s.revert_snapshot(snap);
                    return Ok(None);
                }
            }
            Err(e) => {
                s.pop_discard();
                return Err(e);
            }
        }
    };

    s.add_link(
        offset_pos..offset_pos + 4,
        subtable_idx,
        OffsetWhence::Head,
        0,
        false,
    )?;
    Ok(Some(subtable_idx))
}

fn is_gid_consecutive(
    end_char_code: u32,
    start_char_code: u32,
    gid: GlyphId,
    cp: u32,
    new_gid: GlyphId,
) -> bool {
    cp - 1 == end_char_code && new_gid.to_u32() == gid.to_u32() + (cp - start_char_code)
}

trait Serialize {
    /// serialize cmap subtable
    fn serialize(
        &self,
        s: &mut Serializer,
        plan: &Plan,
        cp_to_new_gid_list: &[(u32, GlyphId)],
    ) -> Result<(), SerializeErrorFlags>;
}

impl Serialize for CmapSubtable<'_> {
    fn serialize(
        &self,
        s: &mut Serializer,
        plan: &Plan,
        cp_to_new_gid_list: &[(u32, GlyphId)],
    ) -> Result<(), SerializeErrorFlags> {
        match self {
            Self::Format4(item) => item.serialize(s, plan, cp_to_new_gid_list),
            Self::Format12(item) => item.serialize(s, plan, cp_to_new_gid_list),
            Self::Format13(item) => item.serialize(s, plan, cp_to_new_gid_list),
            Self::Format14(item) => item.serialize(s, plan, cp_to_new_gid_list),
            Self::Format15(item) => item.serialize(s, plan, cp_to_new_gid_list),
            _ => Ok(()),
        }
    }
}

impl Serialize for Cmap4<'_> {
    fn serialize(
        &self,
        s: &mut Serializer,
        _plan: &Plan,
        cp_to_new_gid_list: &[(u32, GlyphId)],
    ) -> Result<(), SerializeErrorFlags> {
        if cp_to_new_gid_list.is_empty() {
            return Ok(());
        }

        let init_len = s.length();
        // format
        s.embed(4_u16)?;
        // length, initialized to 0
        let length_pos = s.embed(0_u16)?;
        // language
        s.embed(self.language())?;
        // segCountx2, initialized to 0
        let segcount_pos = s.embed(0_u16)?;
        // searchRange, initialized to 0
        let search_range_pos = s.embed(0_u16)?;
        // entrySelector, initialized to 0
        let entry_selector_pos = s.embed(0_u16)?;
        // rangeShift, initialized to 0
        let rangeshift_pos = s.embed(0_u16)?;

        let seg_count = serialize_find_segcount(s, cp_to_new_gid_list);
        let (end_code, start_code, id_delta) =
            serialize_start_end_delta_arrays(s, cp_to_new_gid_list, seg_count as usize)?;
        serialize_rangeoffset_glyph_ids(
            s,
            cp_to_new_gid_list,
            seg_count as usize,
            end_code,
            start_code,
            id_delta,
        )?;

        s.check_assign::<u16>(
            length_pos,
            s.length() - init_len,
            SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
        )?;
        s.check_assign::<u16>(
            segcount_pos,
            seg_count as usize * 2,
            SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
        )?;

        let entry_selector = 1_u16.max(16 - seg_count.leading_zeros() as u16) - 1;
        s.copy_assign(entry_selector_pos, entry_selector);

        let search_range = 2 * (1 << entry_selector) as usize;
        s.check_assign::<u16>(
            search_range_pos,
            search_range,
            SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
        )?;

        let range_shift = if seg_count as usize * 2 > search_range {
            seg_count * 2 - search_range as u16
        } else {
            0
        };
        s.copy_assign(rangeshift_pos, range_shift);

        Ok(())
    }
}

trait UnicodeRangeWriter {
    /// write or count ranges
    fn write_one_range(&mut self, start: u16, end: u16, delta: i16, s: &mut Serializer);
}

#[derive(Default)]
struct RangeCounter {
    seg_count: u16,
}

impl RangeCounter {
    fn new() -> Self {
        RangeCounter::default()
    }

    fn count(&self) -> u16 {
        self.seg_count
    }
}

impl UnicodeRangeWriter for RangeCounter {
    fn write_one_range(&mut self, _start: u16, _end: u16, _delta: i16, _s: &mut Serializer) {
        self.seg_count += 1;
    }
}

// ported from harfbuzz: <https://github.com/harfbuzz/harfbuzz/blob/524e0f0ad582604ad86e04e5a49cd453920f09cb/src/hb-ot-cmap-table.hh#L245>
// trying to make cmap4 packing more optimal:
// it's often possible to save bytes by splitting up existing ranges and encoding parts of them using deltas
// where the cost of splitting the range is less than encoding each glyph individual.
fn to_ranges(
    cp_to_new_gid_list: &[(u32, GlyphId)],
    range_writer: &mut impl UnicodeRangeWriter,
    s: &mut Serializer,
) {
    #[derive(PartialEq)]
    enum Mode {
        FirstSubRange,
        FollowingSubRange,
    }

    let mut start_cp;
    let mut prev_run_start_cp;
    let mut run_start_cp;
    let mut end_cp = 0_u16;
    let mut last_gid;
    let mut run_length;
    let mut delta;
    let mut prev_delta;

    let total_num = cp_to_new_gid_list.len();
    let mut i = 0;

    while i < total_num {
        // Start a new run
        {
            let pair = cp_to_new_gid_list[i];
            start_cp = pair.0 as u16;
            prev_run_start_cp = start_cp;
            run_start_cp = start_cp;
            end_cp = start_cp;
            last_gid = pair.1.to_u32() as u16;
            run_length = 1;
            prev_delta = 0;
        }

        delta = (i32::from(last_gid) - i32::from(start_cp)) as i16;
        let mut mode = Mode::FirstSubRange;
        i += 1;

        while i < total_num {
            // Process range
            let pair = cp_to_new_gid_list[i];
            let next_cp = pair.0 as u16;
            let next_gid = pair.1.to_u32() as u16;
            if next_cp != end_cp + 1 {
                // Current range is over, stop processing.
                break;
            }

            if next_gid == last_gid + 1 {
                // The current run continues.
                end_cp = next_cp;
                run_length += 1;
                last_gid = next_gid;
                i += 1;
                continue;
            }

            // A new run is starting, decide if we want to commit the current run.
            let split_cost = if mode == Mode::FirstSubRange { 8 } else { 16 };
            let run_cost = run_length * 2;
            if run_cost >= split_cost {
                commit_current_range(
                    start_cp,
                    prev_run_start_cp,
                    run_start_cp,
                    end_cp,
                    delta,
                    prev_delta,
                    split_cost,
                    range_writer,
                    s,
                );
                start_cp = next_cp;
            }

            // Start the new run
            mode = Mode::FollowingSubRange;
            prev_run_start_cp = run_start_cp;
            run_start_cp = next_cp;
            end_cp = next_cp;
            prev_delta = delta;
            delta = (i32::from(next_gid) - i32::from(run_start_cp)) as i16;
            run_length = 1;
            last_gid = next_gid;
            i += 1;
        }

        // Finalize range
        commit_current_range(
            start_cp,
            prev_run_start_cp,
            run_start_cp,
            end_cp,
            delta,
            prev_delta,
            8,
            range_writer,
            s,
        );
    }

    if end_cp != 0xFFFF_u16 {
        range_writer.write_one_range(0xFFFF, 0xFFFF, 1, s);
    }
}

/// Writes the current range as either one or two ranges depending on what is most efficient.
#[allow(clippy::too_many_arguments)]
fn commit_current_range(
    start: u16,
    prev_run_start: u16,
    run_start: u16,
    end: u16,
    run_delta: i16,
    previous_run_delta: i16,
    split_cost: u16,
    range_writer: &mut impl UnicodeRangeWriter,
    s: &mut Serializer,
) {
    let should_split = if start < run_start && run_start < end {
        let run_cost = (end - run_start + 1) * 2;
        run_cost >= split_cost
    } else {
        false
    };

    if should_split {
        if start == prev_run_start {
            range_writer.write_one_range(start, run_start - 1, previous_run_delta, s);
        } else {
            range_writer.write_one_range(start, run_start - 1, 0, s);
        }
        range_writer.write_one_range(run_start, end, run_delta, s);
        return;
    }

    if start == run_start {
        // Range is only a run
        range_writer.write_one_range(start, end, run_delta, s);
        return;
    }

    // Write only a single non-run range.
    range_writer.write_one_range(start, end, 0, s);
}

#[derive(Default)]
struct RangeWriter {
    start_code: usize,
    end_code: usize,
    id_delta: usize,
    index: usize,
}

impl RangeWriter {
    fn new(start_code: usize, end_code: usize, id_delta: usize) -> Self {
        Self {
            start_code,
            end_code,
            id_delta,
            ..Default::default()
        }
    }
}

impl UnicodeRangeWriter for RangeWriter {
    fn write_one_range(&mut self, start: u16, end: u16, delta: i16, s: &mut Serializer) {
        let pos = self.index * 2;
        s.copy_assign(self.start_code + pos, start);
        s.copy_assign(self.end_code + pos, end);
        s.copy_assign(self.id_delta + pos, delta);
        self.index += 1;
    }
}

fn serialize_find_segcount(s: &mut Serializer, cp_to_new_gid_list: &[(u32, GlyphId)]) -> u16 {
    let mut counter = RangeCounter::new();
    to_ranges(cp_to_new_gid_list, &mut counter, s);
    counter.count()
}

fn serialize_start_end_delta_arrays(
    s: &mut Serializer,
    cp_to_new_gid_list: &[(u32, GlyphId)],
    seg_count: usize,
) -> Result<(usize, usize, usize), SerializeErrorFlags> {
    let end_code = s.allocate_size(u16::RAW_BYTE_LEN * seg_count, false)?;
    //padding
    s.allocate_size(u16::RAW_BYTE_LEN, true)?;
    let start_code = s.allocate_size(u16::RAW_BYTE_LEN * seg_count, false)?;
    let id_delta = s.allocate_size(u16::RAW_BYTE_LEN * seg_count, false)?;

    let mut writer = RangeWriter::new(start_code, end_code, id_delta);
    to_ranges(cp_to_new_gid_list, &mut writer, s);
    Ok((end_code, start_code, id_delta))
}

fn serialize_rangeoffset_glyph_ids(
    s: &mut Serializer,
    cp_to_new_gid_list: &[(u32, GlyphId)],
    seg_count: usize,
    end_code: usize,
    start_code: usize,
    id_delta: usize,
) -> Result<(), SerializeErrorFlags> {
    let id_range_offset = s.allocate_size(u16::RAW_BYTE_LEN * seg_count, true)?;
    let cp_to_gid_map: FastHashMap<u32, GlyphId> = cp_to_new_gid_list
        .iter()
        .map(|(cp, gid)| (*cp, *gid))
        .collect();

    // Check each segment's idDelta in the same loop that writes its idRangeOffset
    // and glyph ids: those writes never touch the idDelta array, so there's no
    // need to collect the matching segments first.
    for i in 0..seg_count {
        let delta = s
            .get_value_at::<i16>(id_delta + i * 2)
            .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_OTHER)?;
        if delta != 0 {
            continue;
        }
        let val = s.head() - id_range_offset - 2 * i;
        s.copy_assign(id_range_offset + i * 2, val as u16);
        let start_cp = s
            .get_value_at::<u16>(start_code + i * 2)
            .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_OTHER)? as u32;
        let end_cp = s
            .get_value_at::<u16>(end_code + i * 2)
            .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_OTHER)? as u32;
        for cp in start_cp..=end_cp {
            let gid = cp_to_gid_map
                .get(&cp)
                .ok_or(SerializeErrorFlags::SERIALIZE_ERROR_OTHER)?;
            s.embed(gid.to_u32() as u16)?;
        }
    }
    Ok(())
}

impl Serialize for Cmap12<'_> {
    fn serialize(
        &self,
        s: &mut Serializer,
        _plan: &Plan,
        cp_to_new_gid_list: &[(u32, GlyphId)],
    ) -> Result<(), SerializeErrorFlags> {
        serialize_groups(self.format(), self.language(), s, cp_to_new_gid_list)
    }
}

impl Serialize for Cmap13<'_> {
    fn serialize(
        &self,
        s: &mut Serializer,
        _plan: &Plan,
        cp_to_new_gid_list: &[(u32, GlyphId)],
    ) -> Result<(), SerializeErrorFlags> {
        serialize_groups(self.format(), self.language(), s, cp_to_new_gid_list)
    }
}

fn serialize_groups(
    format: u16,
    language: u32,
    s: &mut Serializer,
    cp_to_new_gid_list: &[(u32, GlyphId)],
) -> Result<(), SerializeErrorFlags> {
    if cp_to_new_gid_list.is_empty() {
        return Ok(());
    }
    let init_pos = s.length();
    //copy header format
    s.embed(format)?;
    // reserved
    s.embed(0_u16)?;
    // length, initialized to 0, update later
    let length_pos = s.embed(0_u32)?;
    // language
    s.embed(language)?;
    // numGroups: set to 0 initially
    let num_groups_pos = s.embed(0_u32)?;

    let mut start_char_code = INVALID_UNICODE_CHAR;
    let mut end_char_code = INVALID_UNICODE_CHAR;
    let mut glyph_id = GlyphId::NOTDEF;

    for (cp, gid) in cp_to_new_gid_list.iter() {
        if start_char_code == INVALID_UNICODE_CHAR {
            start_char_code = *cp;
            end_char_code = *cp;
            glyph_id = *gid;
        } else if !(if format == 13 {
            *cp == end_char_code + 1 && *gid == glyph_id
        } else {
            is_gid_consecutive(end_char_code, start_char_code, glyph_id, *cp, *gid)
        }) {
            s.embed(start_char_code)?;
            s.embed(end_char_code)?;
            s.embed(glyph_id.to_u32())?;

            start_char_code = *cp;
            end_char_code = *cp;
            glyph_id = *gid;
        } else {
            end_char_code = *cp;
        }
    }

    s.embed(start_char_code)?;
    s.embed(end_char_code)?;
    s.embed(glyph_id.to_u32())?;

    // update length
    s.check_assign::<u32>(
        length_pos,
        s.length() - init_pos,
        SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
    )?;

    // header size = 16
    s.check_assign::<u32>(
        num_groups_pos,
        (s.length() - init_pos - 16) / SequentialMapGroup::RAW_BYTE_LEN,
        SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
    )?;
    Ok(())
}

// reference: <https://github.com/qxliu76/harfbuzz/blob/1c249be96e27eafd15eb86d832b67fbc3751634b/src/hb-ot-cmap-table.hh#L1369>
macro_rules! serialize_uvs {
    ($table:ident, $selector:ident, $copy_tables:ident) => {
        impl Serialize for $table<'_> {
            fn serialize(
                &self,
                s: &mut Serializer,
                plan: &Plan,
                _cp_to_new_gid_list: &[(u32, GlyphId)],
            ) -> Result<(), SerializeErrorFlags> {
                let snap = s.snapshot();
                let init_len = s.length();
                let init_tail = s.tail();
                //copy header format
                s.embed(self.format())?;
                // length, initialized to 0, update later
                let length_pos = s.embed(0_u32)?;
                // numVarSelectorRecords, initialized to 0, update later
                let num_records_pos = s.embed(0_u32)?;

                if self.var_selector().len() as u64 != self.num_var_selector_records() as u64 {
                    return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
                }
                let retained_records: Vec<&$selector> = self
                    .var_selector()
                    .iter()
                    .filter(|r| plan.unicodes.contains(r.var_selector().to_u32()))
                    .collect();

                let mut obj_indices = Vec::with_capacity(retained_records.len());
                // serializer UVS tables for each variation selector record in reverse order
                // see here for reason: <https://github.com/harfbuzz/harfbuzz/blob/40ef6c05775885241dd3f4d69f08fa4e7e1e451c/src/hb-ot-cmap-table.hh#L1385>
                for record in retained_records.iter().rev() {
                    obj_indices.push($copy_tables(record, self, s, plan)?);
                }

                let mut offset_pos = Vec::with_capacity(obj_indices.len());
                // copy variation selector headers
                for (record, _) in retained_records
                    .iter()
                    .zip(obj_indices.iter().rev())
                    .filter(|(_, (a_idx, b_idx))| a_idx.is_some() || b_idx.is_some())
                {
                    let (default_pos, non_default_pos) =
                        copy_var_selector_record_header(record.var_selector(), s)?;
                    offset_pos.push((default_pos, non_default_pos));
                }

                // subsetted to empty, return
                // 10 is header size of Cmap14
                if s.length() - init_len == 10 {
                    s.revert_snapshot(snap);
                    return Ok(());
                }

                let tail_len = init_tail - s.tail();
                s.check_assign::<u32>(
                    length_pos,
                    s.length() - init_len + tail_len,
                    SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
                )?;
                let num_records = (s.length() - init_len - 10) / $selector::RAW_BYTE_LEN;
                s.check_assign::<u32>(
                    num_records_pos,
                    num_records,
                    SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
                )?;

                // add links to variation records
                for ((default_pos, non_default_pos), (default_obj_idx, non_default_obj_idx)) in
                    offset_pos.iter().zip(
                        obj_indices
                            .iter()
                            .rev()
                            .filter(|(a_idx, b_idx)| a_idx.is_some() || b_idx.is_some()),
                    )
                {
                    if let Some(obj_idx) = default_obj_idx {
                        s.add_link(
                            *default_pos..*default_pos + 4,
                            *obj_idx,
                            OffsetWhence::Head,
                            0,
                            false,
                        )?;
                    }

                    if let Some(obj_idx) = non_default_obj_idx {
                        s.add_link(
                            *non_default_pos..*non_default_pos + 4,
                            *obj_idx,
                            OffsetWhence::Head,
                            0,
                            false,
                        )?;
                    }
                }
                Ok(())
            }
        }
    };
}

serialize_uvs!(
    Cmap14,
    VariationSelector,
    copy_var_selector_record_uvs_tables
);
serialize_uvs!(
    Cmap15,
    VariationSelector15,
    copy_var_selector_record_uvs_tables24
);

fn copy_var_selector_record_header(
    selector: Uint24,
    s: &mut Serializer,
) -> Result<(usize, usize), SerializeErrorFlags> {
    //copy var_selector
    s.embed(selector)?;
    // offset to default UVS, initialized to 0
    let default_pos = s.embed(0_u32)?;
    // offset to non-default UVS, initialized to 0
    let non_default_pos = s.embed(0_u32)?;
    Ok((default_pos, non_default_pos))
}

macro_rules! copy_uvs_tables {
    ($name:ident, $selector:ident, $table:ident, $copy_non_default:ident) => {
        fn $name(
            record: &$selector,
            cmap: &$table,
            s: &mut Serializer,
            plan: &Plan,
        ) -> Result<(Option<ObjIdx>, Option<ObjIdx>), SerializeErrorFlags> {
            let mut non_default_uvs_obj_idx = None;
            if let Some(non_default_uvs) = record
                .non_default_uvs(cmap.offset_data())
                .transpose()
                .ok()
                .flatten()
            {
                s.push()?;
                let num = $copy_non_default(&non_default_uvs, s, plan)?;
                if num == 0 {
                    s.pop_discard();
                } else {
                    non_default_uvs_obj_idx = s.pop_pack(true);
                }
            }

            let mut default_uvs_obj_idx = None;
            if let Some(default_uvs) = record
                .default_uvs(cmap.offset_data())
                .transpose()
                .ok()
                .flatten()
            {
                s.push()?;
                let num = copy_default_uvs(&default_uvs, s, plan)?;
                if num == 0 {
                    s.pop_discard();
                } else {
                    default_uvs_obj_idx = s.pop_pack(true);
                }
            }
            Ok((default_uvs_obj_idx, non_default_uvs_obj_idx))
        }
    };
}

copy_uvs_tables!(
    copy_var_selector_record_uvs_tables,
    VariationSelector,
    Cmap14,
    copy_non_default_uvs
);
copy_uvs_tables!(
    copy_var_selector_record_uvs_tables24,
    VariationSelector15,
    Cmap15,
    copy_non_default_uvs24
);

macro_rules! copy_non_default_uvs {
    ($name:ident, $table:ident, $wide:expr) => {
        fn $name(
            non_default_uvs: &$table,
            s: &mut Serializer,
            plan: &Plan,
        ) -> Result<u32, SerializeErrorFlags> {
            // num_uvs_mapping, initialized to 0
            let num_pos = s.embed(0_u32)?;
            let mut num: u32 = 0;
            if non_default_uvs.uvs_mapping().len() as u64
                != non_default_uvs.num_uvs_mappings() as u64
            {
                return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_READ_ERROR));
            }
            for uvs_mapping in non_default_uvs.uvs_mapping().iter() {
                let old_gid = GlyphId::new(uvs_mapping.glyph_id().into());
                if !plan.unicodes.contains(uvs_mapping.unicode_value().to_u32())
                    && !plan.glyphs_requested.contains(old_gid)
                {
                    continue;
                }
                let Some(new_gid) = plan.glyph_map.get(&old_gid) else {
                    continue;
                };
                copy_uvs_mapping(uvs_mapping.unicode_value(), *new_gid, $wide, s)?;
                num += 1;
            }

            if num == 0 {
                return Ok(num);
            }
            s.copy_assign(num_pos, num);
            Ok(num)
        }
    };
}

copy_non_default_uvs!(copy_non_default_uvs, NonDefaultUvs, false);
copy_non_default_uvs!(copy_non_default_uvs24, NonDefaultUvs24, true);

fn copy_uvs_mapping(
    unicode_value: Uint24,
    glyph_id: GlyphId,
    wide: bool,
    s: &mut Serializer,
) -> Result<(), SerializeErrorFlags> {
    s.embed(unicode_value)?;
    if wide {
        let glyph_id = Uint24::try_from(glyph_id.to_u32() as usize)
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.embed(glyph_id)?;
    } else {
        let glyph_id = u16::try_from(glyph_id.to_u32())
            .map_err(|_| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW))?;
        s.embed(glyph_id)?;
    }
    Ok(())
}

fn copy_default_uvs(
    default_uvs: &DefaultUvs,
    s: &mut Serializer,
    plan: &Plan,
) -> Result<u32, SerializeErrorFlags> {
    let snap = s.snapshot();
    // numUnicodeValueRanges, initialized to 0
    let len_pos = s.embed(0_u32)?;

    let init_len = s.length();
    let org_num_range = default_uvs.num_unicode_value_ranges() as usize;
    let num_bits = 32 - (org_num_range as u32).leading_zeros() as usize;
    let org_unicode_ranges = default_uvs.ranges();
    if org_num_range > plan.unicodes.len() as usize * num_bits {
        let mut start = INVALID_UNICODE_CHAR;
        let mut end = INVALID_UNICODE_CHAR;

        for u in plan.unicodes.iter() {
            // search range if a codepoint lies anywhere in [start, start + additional_count].
            if org_unicode_ranges
                .binary_search_by(|r| {
                    let start = r.start_unicode_value().to_u32();
                    if u < start {
                        core::cmp::Ordering::Greater
                    } else if u > start + r.additional_count() as u32 {
                        core::cmp::Ordering::Less
                    } else {
                        core::cmp::Ordering::Equal
                    }
                })
                .is_err()
            {
                continue;
            }

            if start == INVALID_UNICODE_CHAR {
                start = u;
                end = u;
                continue;
            }

            if end + 1 != u || end - start == 255 {
                s.embed(Uint24::new(start))?;
                s.embed((end - start) as u8)?;
                start = u;
            }
            end = u;
        }

        if start != INVALID_UNICODE_CHAR {
            s.embed(Uint24::new(start))?;
            s.embed((end - start) as u8)?;
        }
    } else {
        let mut last_code = INVALID_UNICODE_CHAR;
        // `count` tracks the number of codepoints accumulated in the current run
        // minus one, i.e. the run's `additionalCount`.
        //
        // A run is flushed once `count` reaches u8::MAX, since `additionalCount`
        // is a u8 and a merged run can span more than 256 codepoints when
        // adjacent source ranges join up. This matches the split that the other
        // branch above already performs via `end - start == 255`.
        let mut count = 0_u8;
        for unicode_range in default_uvs.ranges() {
            let mut cur_entry = unicode_range.start_unicode_value().to_u32().wrapping_sub(1);
            let end = cur_entry.wrapping_add(unicode_range.additional_count() as u32 + 2);

            while let Some(entry) = plan.unicodes.iter_after(cur_entry).next() {
                if entry >= end {
                    break;
                }

                cur_entry = entry;
                if last_code == INVALID_UNICODE_CHAR {
                    last_code = entry;
                    count = 0;
                } else if count == u8::MAX || last_code + count as u32 + 1 != entry {
                    s.embed(Uint24::new(last_code))?;
                    s.embed(count)?;

                    last_code = entry;
                    count = 0;
                } else {
                    count += 1;
                }
            }
        }

        if last_code != INVALID_UNICODE_CHAR {
            s.embed(Uint24::new(last_code))?;
            s.embed(count)?;
        }
    }

    // return if subsetted to empty
    if s.length() == init_len {
        s.revert_snapshot(snap);
        return Ok(0);
    }

    let num_ranges = (s.length() - init_len) / UnicodeRange::RAW_BYTE_LEN;
    s.check_assign::<u32>(
        len_pos,
        num_ranges,
        SerializeErrorFlags::SERIALIZE_ERROR_INT_OVERFLOW,
    )?;
    Ok(num_ranges as u32)
}

#[allow(dead_code)]
pub(crate) struct SubtableUnicodeCache {
    base: usize,
    cached_unicodes: FastHashMap<usize, IntSet<u32>>,
}

impl SubtableUnicodeCache {
    fn new(base: usize) -> Self {
        Self {
            base,
            cached_unicodes: FastHashMap::default(),
        }
    }

    fn set_for(
        &mut self,
        record_idx: usize,
        cmap_subtable: &CmapSubtable,
        num_glyphs: usize,
    ) -> Option<&IntSet<u32>> {
        self.cached_unicodes.entry(record_idx).or_insert_with(|| {
            let mut subtable_unicodes_set = IntSet::empty();
            cmap_subtable.collect_unicodes(num_glyphs, &mut subtable_unicodes_set);
            subtable_unicodes_set
        });
        self.cached_unicodes.get(&record_idx)
    }
}

trait CollectUnicodes {
    /// collect unicodes
    fn collect_unicodes(&self, num_glyphs: usize, out: &mut IntSet<u32>);
}

impl CollectUnicodes for CmapSubtable<'_> {
    fn collect_unicodes(&self, num_glyphs: usize, out: &mut IntSet<u32>) {
        match self {
            Self::Format4(item) => item.collect_unicodes(num_glyphs, out),
            Self::Format12(item) => item.collect_unicodes(num_glyphs, out),
            _ => (),
        }
    }
}

impl CollectUnicodes for Cmap4<'_> {
    fn collect_unicodes(&self, _num_glyphs: usize, out: &mut IntSet<u32>) {
        let id_deltas = self.id_delta();
        let seg_count = self.seg_count_x2() / 2;
        let glyph_id_array = self.glyph_id_array();
        for (i, ((start, end), range_offset)) in self
            .start_code()
            .iter()
            .zip(self.end_code())
            .zip(self.id_range_offsets())
            .enumerate()
        {
            let start = start.get() as u32;
            if start == 0xFFFF {
                break;
            }

            let end = end.get() as u32;
            let range_offset = range_offset.get() as u32;
            out.insert_range(start..=end);
            if range_offset == 0 {
                for cp in start..=end {
                    let gid = (cp as u16).wrapping_add_signed(id_deltas[i].get());
                    if gid == 0 {
                        out.remove(cp);
                    }
                }
            } else {
                for cp in start..=end {
                    let index = range_offset / 2 + (cp - start) + i as u32 - seg_count as u32;
                    if index as usize >= glyph_id_array.len() {
                        out.remove_range(cp..=end);
                        break;
                    }
                    let Some(gid) = glyph_id_array.get(index as usize) else {
                        out.remove(cp);
                        continue;
                    };

                    if gid.get() == 0 {
                        out.remove(cp);
                    }
                }
            }
        }
    }
}

impl CollectUnicodes for Cmap12<'_> {
    fn collect_unicodes(&self, num_glyphs: usize, out: &mut IntSet<u32>) {
        for group in self.groups() {
            let mut start = group.start_char_code();
            let mut end = group.end_char_code().min(UNICODE_MAX);
            let mut gid = group.start_glyph_id();
            if gid == 0 {
                start += 1;
                gid += 1;
            }

            if gid as usize >= num_glyphs {
                continue;
            }

            if (gid + end - start) as usize >= num_glyphs {
                end = UNICODE_MAX.min(start + num_glyphs as u32 - gid);
            }
            out.insert_range(start..=end);
        }
    }
}

#[cfg(test)]
#[path = "cmap/uvs_tests.rs"]
mod uvs_tests;

#[cfg(test)]
#[path = "cmap/dmap_tests.rs"]
mod dmap_tests;
