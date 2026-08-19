//! splitting layout (GPOS) subtables
//!
//! The implementations here are directly adapted from the code in hb-repacker,
//! with a few minor differences:
//!
//! - we don't use a context/interface for the tables, instead using a generic
//!   'split_subtables' method similar to 'actuate_subtable_split' and passing
//!   it a method for each lookup type
//! - harfbuzz splits off new subtables but retains the original subtable,
//!   shrinking it to fit. We skip this step, and generate all new subtables.

use std::collections::HashMap;

use font_types::GlyphId16;
use read_fonts::tables::{
    gpos::{self as rgpos},
    layout as rlayout,
};

use super::Graph;
use crate::{
    object::ObjectId, tables::layout as wlayout, write::TableData, FontWrite, TableWriter,
};

mod mark2base;
mod pairpos;

pub(super) use mark2base::split_mark_to_base;
pub(super) use pairpos::split_pair_pos;

const MAX_TABLE_SIZE: usize = u16::MAX as usize;

/// A common impl handling updating the lookup with the new subtables
///
/// This is roughly equivalent to `actuate_subtable_split` in hb-repacker:
/// <https://github.com/harfbuzz/harfbuzz/blob/d5cb1a315380e9bd78ff377a586b78bc42abafa6/src/graph/split-helpers.hh#L34>
fn split_subtables(
    graph: &mut Graph,
    lookup: ObjectId,
    split_fn: fn(&mut Graph, ObjectId) -> Option<Vec<ObjectId>>,
) {
    let data = graph.objects.remove(&lookup).unwrap();
    debug_assert!(
        data.reparse::<rgpos::PositionLookup>().is_ok(),
        "table splitting is only relevant for GPOS?"
    );
    log::debug!("trying to split subtables in '{}'", data.type_);

    let mut new_subtables = HashMap::new();
    for (i, subtable) in data.offsets.iter().enumerate() {
        if let Some(split_subtables) = split_fn(graph, subtable.object) {
            log::trace!("produced {} splits for subtable {i}", split_subtables.len());
            new_subtables.insert(subtable.object, split_subtables);
        }
    }

    if new_subtables.is_empty() {
        // just put the old data back unchanged; nothing to see here
        graph.objects.insert(lookup, data);
        log::debug!("Splitting produced no new subtables");
        return;
    }

    let n_new_subtables = new_subtables
        .values()
        // - 1 because each group of new subtables replaces an old subtable
        .map(|ids| ids.len() - 1)
        .sum::<usize>();
    log::debug!("Splitting produced {n_new_subtables} new subtables");

    let n_total_subtables: u16 = (data.offsets.len() + n_new_subtables).try_into().unwrap();
    // we just want the lookup type/flag/etc, but we need a generic FontRead type
    let generic_lookup: rlayout::Lookup<()> = data.reparse().unwrap();
    let mut new_data = TableData::new(data.type_);
    new_data.write(generic_lookup.lookup_type());
    new_data.write(generic_lookup.lookup_flag());
    new_data.write(n_total_subtables);
    let old_mark_filter_set = generic_lookup.mark_filtering_set();
    for sub in data.offsets {
        match new_subtables.get(&sub.object) {
            Some(new) => new.iter().for_each(|id| new_data.add_offset(*id, 2, 0)),
            None => new_data.add_offset(sub.object, 2, 0),
        }
    }
    if let Some(mark_filtering_set) = old_mark_filter_set {
        new_data.write(mark_filtering_set);
    }

    graph.nodes.get_mut(&lookup).unwrap().size = new_data.bytes.len() as _;
    graph.objects.insert(lookup, new_data);
}

fn split_coverage(coverage: &rlayout::CoverageTable, start: u16, end: u16) -> TableData {
    assert!(start <= end);
    let len = end - start;
    let mut data = TableData::default();
    match coverage {
        rlayout::CoverageTable::Format1(table) => {
            data.write(1u16);
            data.write(len);
            for gid in &table.glyph_array()[start as usize..end as usize] {
                data.write(gid.get());
            }
        }
        rlayout::CoverageTable::Format2(table) => {
            // we will stay in format2, but it's possible it is no longer best?
            let records = table
                .range_records()
                .iter()
                .filter_map(|record| split_range_record(record, start, end - 1))
                .collect::<Vec<_>>();
            data.write(2u16);
            data.write(records.len() as u16);
            for record in records {
                data.write(record.start_glyph_id);
                data.write(record.end_glyph_id);
                data.write(record.start_coverage_index);
            }
        }
    }
    data
}

// NOTE: range records use inclusive ranges, everything else here is exclusive
fn split_range_record(
    record: &rlayout::RangeRecord,
    start: u16,
    end: u16,
) -> Option<wlayout::RangeRecord> {
    // the range is a range of coverage indices, not of glyph ids!
    let cov_start = record.start_coverage_index();
    let len = record.end_glyph_id().to_u16() - record.start_glyph_id().to_u16();
    let cov_range = cov_start..cov_start + len;

    if cov_range.start > end || cov_range.end < start {
        return None;
    }

    // okay, so we intersect. what is our start_coverage_index?

    // the new start is the number of items in the subset range that occur
    // before the first item in this record
    let new_cov_start = cov_range.start.saturating_sub(start);

    let start_glyph_delta = start.saturating_sub(cov_range.start);
    // the start is the old start + the number of glyphs truncated from the record
    let start_glyph = record.start_glyph_id().to_u16() + start_glyph_delta;
    let range_len = cov_range.end.min(end) - cov_range.start.max(start);

    let end_glyph = start_glyph + range_len;
    Some(wlayout::RangeRecord::new(
        GlyphId16::new(start_glyph),
        GlyphId16::new(end_glyph),
        new_cov_start,
    ))
}

// a helper to convert a write-fonts table into graph-ready bytes.
//
// NOTE: the table must not contain any offsets. intended for coverage/classdef
fn make_table_data(table: &dyn FontWrite) -> TableData {
    let mut writer = TableWriter::default();
    table.write_into(&mut writer);

    let mut r = writer.into_data();
    r.type_ = table.table_type();
    r
}

#[cfg(test)]
mod tests {
    use std::ops::Range;

    use super::*;

    fn make_read_record(start_coverage_index: u16, glyphs: Range<u16>) -> rlayout::RangeRecord {
        rlayout::RangeRecord {
            start_glyph_id: GlyphId16::new(glyphs.start).into(),
            end_glyph_id: GlyphId16::new(glyphs.end).into(),
            start_coverage_index: start_coverage_index.into(),
        }
    }

    #[test]
    fn splitting_range_records() {
        let record = make_read_record(10, 20..30);
        // fully before: no result
        assert!(split_range_record(&record, 0, 5).is_none());

        // just first item:
        let split = split_range_record(&record, 5, 10).unwrap();
        assert_eq!(split.start_glyph_id.to_u16(), 20);
        assert_eq!(split.end_glyph_id.to_u16(), 20);
        assert_eq!(split.start_coverage_index, 5);

        // overlapping at start
        let split = split_range_record(&record, 8, 12).unwrap();
        assert_eq!(split.start_glyph_id.to_u16(), 20);
        assert_eq!(split.end_glyph_id.to_u16(), 22);
        assert_eq!(split.start_coverage_index, 2);

        // range is interior
        let split = split_range_record(&record, 12, 15).unwrap();
        assert_eq!(split.start_glyph_id.to_u16(), 22);
        assert_eq!(split.end_glyph_id.to_u16(), 25);
        assert_eq!(split.start_coverage_index, 0);

        // overlapping at end
        let split = split_range_record(&record, 18, 32).unwrap();
        assert_eq!(split.start_glyph_id.to_u16(), 28);
        assert_eq!(split.end_glyph_id.to_u16(), 30);
        assert_eq!(split.start_coverage_index, 0);

        // fully covered
        let split = split_range_record(&record, 5, 32).unwrap();
        assert_eq!(split.start_glyph_id.to_u16(), 20);
        assert_eq!(split.end_glyph_id.to_u16(), 30);
        assert_eq!(split.start_coverage_index, 5);

        // identical
        let split = split_range_record(&record, 10, 20).unwrap();
        assert_eq!(split.start_glyph_id.to_u16(), 20);
        assert_eq!(split.end_glyph_id.to_u16(), 30);
        assert_eq!(split.start_coverage_index, 0);

        // fully after
        assert!(split_range_record(&record, 30, 35).is_none());
    }

    #[test]
    fn simple_split_at_end() {
        let record = make_read_record(0, 0..4);
        let split = split_range_record(&record, 2, 6).unwrap();
        assert_eq!(split.start_glyph_id.to_u16(), 2);
        assert_eq!(split.end_glyph_id.to_u16(), 4);
    }

    #[test]
    fn split_inclusive() {
        let record = make_read_record(0, 0..100);
        let head = split_range_record(&record, 0, 50).unwrap();
        assert_eq!(head.start_glyph_id.to_u16(), 0);
        assert_eq!(head.end_glyph_id.to_u16(), 50);
        let tail = split_range_record(&record, 50, 100).unwrap();
        assert_eq!(tail.start_glyph_id.to_u16(), 50);
        assert_eq!(tail.end_glyph_id.to_u16(), 100);
    }
}
