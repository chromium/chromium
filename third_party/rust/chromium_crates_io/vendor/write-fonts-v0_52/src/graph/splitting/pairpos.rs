//! splitting of PairPos subtables

use std::collections::{BTreeSet, HashMap, HashSet};

use font_types::{FixedSize, GlyphId16, Offset16};
use read_fonts::tables::{
    gpos::{self as rgpos, ValueFormat},
    layout as rlayout,
};

use super::{Graph, ObjectId};
use crate::{tables::layout as wlayout, write::OffsetRecord, write::TableData};

pub(crate) fn split_pair_pos(graph: &mut Graph, lookup: ObjectId) {
    super::split_subtables(graph, lookup, split_pair_pos_subtable)
}

fn split_pair_pos_subtable(graph: &mut Graph, lookup: ObjectId) -> Option<Vec<ObjectId>> {
    let data = &graph.objects[&lookup];
    let format: u16 = data.read_at(0).unwrap();
    match format {
        1 => split_pair_pos_format_1(graph, lookup),
        2 => split_pair_pos_format_2(graph, lookup),
        other => {
            log::warn!("unexpected pairpos format '{other}'");
            None
        }
    }
}

// based off of
// <https://github.com/harfbuzz/harfbuzz/blob/5d543d64222c6ce45332d0c188790f90691ef112/src/graph/pairpos-graph.hh#L50>
fn split_pair_pos_format_1(graph: &mut Graph, subtable: ObjectId) -> Option<Vec<ObjectId>> {
    const BASE_SIZE: usize = 5 * u16::RAW_BYTE_LEN;

    let data = &graph.objects[&subtable];

    debug_assert!(data.reparse::<rgpos::PairPosFormat1>().is_ok());
    let coverage_id = data.offsets.first().unwrap();
    assert_eq!(coverage_id.pos, 2, "offset records are always sorted");
    let coverage_size = graph.objects[&coverage_id.object].bytes.len();

    let mut visited = HashSet::with_capacity(data.offsets.len());

    let mut partial_coverage_size = 4;
    let mut accumulated = BASE_SIZE;
    let mut split_points = Vec::new();

    for (i, pair_set_off) in data.offsets.iter().skip(1).enumerate() {
        let table_size = if visited.insert(pair_set_off.object) {
            let pairset = &graph.objects[&pair_set_off.object];
            // the size of the pairset table itself
            let mut subgraph_size = pairset.bytes.len();
            // now we add the lengths of any new device tables. we do *not*
            // deduplicate these, even within the graph, since it is possible
            // that they will need to be re-duplicated later in some cases?
            // TODO: investigate whether there are any wins here?
            // <https://github.com/googlefonts/fontations/issues/596>
            subgraph_size += pairset
                .offsets
                .iter()
                .map(|off| graph.objects[&off.object].bytes.len())
                .sum::<usize>();
            subgraph_size
        } else {
            0
        };
        let accumulated_delta = table_size +
            // the offset to the table
            Offset16::RAW_BYTE_LEN;
        // another glyph in the coverage table
        partial_coverage_size += u16::RAW_BYTE_LEN;
        accumulated += accumulated_delta;
        let total = accumulated + coverage_size.min(partial_coverage_size);
        if total > super::MAX_TABLE_SIZE {
            log::trace!("adding split at {i}");
            split_points.push(i);
            accumulated = BASE_SIZE + accumulated_delta;
            partial_coverage_size = 6; // + one glyph, because this table didn't fit
            visited.clear();
        }
    }

    log::debug!(
        "nothing to split, size '{}'",
        accumulated + coverage_size.min(partial_coverage_size)
    );

    if split_points.is_empty() {
        return None;
    }

    split_points.push(data.offsets.len() - 1);
    // okay, now we have a list of split points.

    let mut new_subtables = Vec::new();
    let mut prev_split = 0;
    for next_split in split_points {
        // the split point is the *start* of the next subtable, so we do not
        // include this item in this subtable
        let new_subtable = split_off_ppf1(graph, subtable, prev_split, next_split);
        prev_split = next_split;
        new_subtables.push(graph.add_object(new_subtable));
    }
    Some(new_subtables)
}

fn split_off_ppf1(graph: &mut Graph, subtable: ObjectId, start: usize, end: usize) -> TableData {
    let coverage = graph.objects[&subtable].offsets.first().unwrap().object;
    let coverage = graph.objects.get(&coverage).unwrap();
    let coverage = coverage.reparse::<rlayout::CoverageTable>().unwrap();
    let n_pair_sets = end - start;
    let new_coverage = super::split_coverage(&coverage, start as u16, end as u16);
    let new_cov_id = graph.add_object(new_coverage);

    let data = &graph.objects[&subtable];
    let table = data.reparse::<rgpos::PairPosFormat1>().unwrap();

    let mut new_ppf1 = TableData::new(data.type_);

    new_ppf1.write(table.pos_format());
    new_ppf1.add_offset(new_cov_id, 2, 0);
    new_ppf1.write(table.value_format1());
    new_ppf1.write(table.value_format2());
    new_ppf1.write(n_pair_sets as u16);
    for off in data.offsets[1 + start..].iter().take(n_pair_sets as _) {
        new_ppf1.add_offset(off.object, 2, 0)
    }
    new_ppf1
}

// based off of
// <https://github.com/harfbuzz/harfbuzz/blob/f380a32825a1b2c51bbe21dc7acb9b3cc0921f69/src/graph/pairpos-graph.hh#L207>
fn split_pair_pos_format_2(graph: &mut Graph, subtable: ObjectId) -> Option<Vec<ObjectId>> {
    // the minimum size of a format 2 subtable
    const BASE_SIZE: usize = 8 * u16::RAW_BYTE_LEN;
    let data = &graph.objects[&subtable];

    let pp2 = data.reparse::<rgpos::PairPosFormat2>().unwrap();
    let cur_len = data.bytes.len();
    log::info!(
        "PairPos f.2 subtable has {} class1 and {} class2, current size {cur_len} ",
        pp2.class1_count(),
        pp2.class2_count()
    );
    // we can't get these from the reparsed table because its offsets
    // are not valid until compiled into the final table
    let coverage_id = data.offsets[0].object;
    let class_def1_id = data.offsets[1].object;
    let class_def2_id = data.offsets[2].object;
    let class_def2_size = graph.objects[&class_def2_id].bytes.len();
    let coverage = &graph.objects[&coverage_id];
    let coverage = coverage.reparse::<rlayout::CoverageTable>().unwrap();

    let class_def1 = &graph.objects[&class_def1_id];
    let class_def1 = class_def1.reparse::<rlayout::ClassDef>().unwrap();
    let estimator = ClassDefSizeEstimator::new(coverage, class_def1);

    let class2_count = pp2.class2_count();
    let class1_record_size = class2_count as usize
        * (pp2.value_format1().record_byte_len() + pp2.value_format2().record_byte_len());

    let mut accumulated = BASE_SIZE;
    let mut coverage_size = 4;
    let mut class_def_1_size = 4;

    let mut split_points = Vec::new();
    let has_device_tables = (pp2.value_format1() | pp2.value_format2())
        .intersects(rgpos::ValueFormat::ANY_DEVICE_OR_VARIDX);

    let mut visited = HashSet::new();
    let mut next_device_offset = 3; // start after coverage + class defs
    for (idx, class1rec) in pp2.class1_records().iter().enumerate() {
        let mut accumulated_delta = class1_record_size;
        coverage_size += estimator.increment_coverage_size(idx as _);
        class_def_1_size += estimator.increment_class_def_size(idx as _);

        if has_device_tables {
            for class2rec in class1rec.unwrap().class2_records.iter() {
                let class2rec = class2rec.as_ref().unwrap();
                accumulated_delta += size_of_value_record_children(
                    &class2rec.value_record1,
                    graph,
                    &data.offsets,
                    &mut next_device_offset,
                    &mut visited,
                );
                accumulated_delta += size_of_value_record_children(
                    &class2rec.value_record2,
                    graph,
                    &data.offsets,
                    &mut next_device_offset,
                    &mut visited,
                );
            }
        }

        accumulated += accumulated_delta;
        let largest_obj = coverage_size.max(class_def_1_size).max(class_def2_size);
        let total = accumulated + coverage_size + class_def_1_size + class_def2_size
        // largest obj packs last and can overflow (we only point to the start)
        - largest_obj;

        if total > super::MAX_TABLE_SIZE {
            split_points.push(idx);
            // split does not include this class, so add it for the next iteration
            accumulated = BASE_SIZE + accumulated_delta;
            coverage_size = 4 + estimator.increment_coverage_size(idx as _);
            class_def_1_size = 4 + estimator.increment_class_def_size(idx as _);
            visited.clear();
        }
    }

    log::debug!("identified {} split points", split_points.len());
    if split_points.is_empty() {
        return None;
    }

    split_points.push(pp2.class1_count() as usize);
    // now we have a list of split points, and just need to do the splitting.
    // note: harfbuzz does a thing here with a context type and an 'actuate_splits'
    // method.

    let mut new_subtables = Vec::new();
    let mut prev_split = 0;
    let mut next_device_offset = 3; // after coverage & two class defs
    for next_split in split_points {
        let (new_subtable, offsets_used) =
            split_off_ppf2(graph, subtable, prev_split, next_split, next_device_offset);
        prev_split = next_split;
        next_device_offset += offsets_used;
        new_subtables.push(graph.add_object(new_subtable));
    }

    Some(new_subtables)
}

// returns the new table, + the number of non-null device offsets encountered.
fn split_off_ppf2(
    graph: &mut Graph,
    subtable: ObjectId,
    start: usize,
    end: usize,
    first_device_idx: usize,
) -> (TableData, usize) {
    // we have to do this bit manually (instead of via reparsing) because of borrowk
    let coverage = graph.objects[&subtable].offsets.first().unwrap().object;
    let coverage = graph.objects.get(&coverage).unwrap();
    let coverage = coverage.reparse::<rlayout::CoverageTable>().unwrap();
    let class_def_1 = graph.objects[&subtable].offsets[1].object;
    let class_def_1 = graph.objects.get(&class_def_1).unwrap();
    let class_def_1 = class_def_1.reparse::<rlayout::ClassDef>().unwrap();

    let class1_count = end - start;
    log::trace!("splitting off {class1_count} class1records ({start}..={end})");

    let class_map = coverage
        .iter()
        .filter_map(|gid| {
            let glyph_class = class_def_1.get(gid);
            (start..end)
                .contains(&(glyph_class as usize))
                // classes are used as indexes, so adjust them
                .then_some((gid, glyph_class.saturating_sub(start as u16)))
        })
        .collect::<HashMap<_, _>>();

    let new_coverage = class_map
        .keys()
        .copied()
        .collect::<wlayout::CoverageTable>();
    let new_coverage = super::make_table_data(&new_coverage);
    let new_cov_id = graph.add_object(new_coverage);
    let new_class_def1 = class_map
        .iter()
        .map(|tup| (*tup.0, *tup.1))
        .collect::<wlayout::ClassDef>();
    let new_class_def1 = super::make_table_data(&new_class_def1);
    let new_class_def1_id = graph.add_object(new_class_def1);
    // we reuse class2 without changing it. maybe we could be changing it?
    let class_def_2_id = graph.objects[&subtable].offsets[2].object;

    let data = &graph.objects[&subtable];
    let table = data.reparse::<rgpos::PairPosFormat2>().unwrap();
    let value_format1 = table.value_format1();
    let value_format2 = table.value_format2();

    let mut new_ppf2 = TableData::new(data.type_);
    new_ppf2.write(table.pos_format());
    new_ppf2.add_offset(new_cov_id, 2, 0);
    new_ppf2.write(value_format1);
    new_ppf2.write(value_format2);
    new_ppf2.add_offset(new_class_def1_id, 2, 0);
    new_ppf2.add_offset(class_def_2_id, 2, 0);
    new_ppf2.write(class1_count as u16);
    new_ppf2.write(table.class2_count());

    // now we need to copy over the class1records
    let mut seen_offsets = 0;
    for class2rec in table
        .class1_records()
        .iter()
        .skip(start)
        .take(class1_count)
        .flat_map(|c1rec| {
            c1rec
                .unwrap()
                .class2_records()
                .iter()
                .map(|rec| rec.unwrap())
        })
    {
        let rec_offset_start = first_device_idx + seen_offsets;
        let rec_offsets = &graph.objects[&subtable].offsets[rec_offset_start..];
        let rec1_seen = copy_value_rec(
            &mut new_ppf2,
            class2rec.value_record1(),
            value_format1,
            rec_offsets,
        );
        let rec_offsets = &rec_offsets[rec1_seen..];
        seen_offsets += rec1_seen;
        seen_offsets += copy_value_rec(
            &mut new_ppf2,
            class2rec.value_record2(),
            value_format2,
            rec_offsets,
        );
    }
    (new_ppf2, seen_offsets)
}

// returns the number of non-null offsets encountered in this record
fn copy_value_rec(
    target: &mut TableData,
    rec: &rgpos::ValueRecord,
    format: ValueFormat,
    dev_offsets: &[OffsetRecord],
) -> usize {
    let mut seen_offsets = 0;
    // a little macro to help us copy over all the fields.
    // - first we copy over the non-device tables
    // - then for the device tables, if they are present we copy over
    // the id.
    macro_rules! write_opt_field {
        ($fld:ident) => {
            if let Some(val) = rec.$fld() {
                target.write(val);
            }
        };
        ($fld:ident, $flag:expr) => {
            if !rec.$fld.get().is_null() {
                // we write this in a funny way to dodge a clippy warning
                seen_offsets += 1;
                target.add_offset(dev_offsets[seen_offsets - 1].object, 2, 0);
            } else if $flag {
                target.write(0u16); // null offset
            }
        };
    }
    write_opt_field!(x_placement);
    write_opt_field!(y_placement);
    write_opt_field!(x_advance);
    write_opt_field!(y_advance);

    write_opt_field!(
        x_placement_device,
        format.contains(ValueFormat::X_PLACEMENT_DEVICE)
    );
    write_opt_field!(
        y_placement_device,
        format.contains(ValueFormat::Y_PLACEMENT_DEVICE)
    );
    write_opt_field!(
        x_advance_device,
        format.contains(ValueFormat::X_ADVANCE_DEVICE)
    );
    write_opt_field!(
        y_advance_device,
        format.contains(ValueFormat::Y_ADVANCE_DEVICE)
    );
    seen_offsets
}

struct ClassDefSizeEstimator {
    consecutive_gids: bool,
    num_ranges_per_class: HashMap<u16, u16>,
    glyphs_per_class: HashMap<u16, BTreeSet<GlyphId16>>,
}

const GLYPH_SIZE: usize = std::mem::size_of::<u16>();

impl ClassDefSizeEstimator {
    fn new(coverage: rlayout::CoverageTable, classdef: rlayout::ClassDef) -> Self {
        let mut consecutive_gids = true;
        let mut last_gid = None;
        let mut glyphs_per_class = HashMap::new();
        for (gid, class) in coverage.iter().map(|gid| (gid, classdef.get(gid))) {
            if let Some(last) = last_gid.take() {
                if last + 1 != gid.to_u16() {
                    consecutive_gids = false;
                }
            }
            last_gid = Some(gid.to_u16());
            glyphs_per_class
                .entry(class)
                .or_insert(BTreeSet::default())
                .insert(gid);
        }

        // now compute the number of ranges manually, skipping class 0
        let mut num_ranges_per_class = HashMap::with_capacity(glyphs_per_class.len());
        for (class, glyphs) in glyphs_per_class.iter().filter(|x| *x.0 != 0) {
            let num_ranges = count_num_ranges(glyphs);
            num_ranges_per_class.insert(*class, num_ranges);
        }
        ClassDefSizeEstimator {
            consecutive_gids,
            num_ranges_per_class,
            glyphs_per_class,
        }
    }

    fn n_glyphs_in_class(&self, class: u16) -> usize {
        self.glyphs_per_class
            .get(&class)
            .map(BTreeSet::len)
            .unwrap_or_default()
    }

    fn increment_coverage_size(&self, class: u16) -> usize {
        GLYPH_SIZE * self.n_glyphs_in_class(class)
    }

    fn increment_class_def_size(&self, class: u16) -> usize {
        // classdef2 uses 6 bytes for each range (start, end, class)
        const SIZE_PER_RANGE: usize = 6;
        let class_def_2_size = SIZE_PER_RANGE
            * self
                .num_ranges_per_class
                .get(&class)
                .copied()
                .unwrap_or_default() as usize;
        if self.consecutive_gids {
            class_def_2_size.min(self.n_glyphs_in_class(class) * GLYPH_SIZE)
        } else {
            class_def_2_size
        }
    }
}

fn count_num_ranges(glyphs: &BTreeSet<GlyphId16>) -> u16 {
    let mut count = 0;
    let mut last = None;
    for gid in glyphs {
        match (last.take(), gid.to_u16()) {
            (Some(prev), current) if current == prev + 1 => (), // in same range
            _ => count += 1, // first glyph or glyph that starts new range
        }
        last = Some(gid.to_u16());
    }
    count
}

fn size_of_value_record_children(
    record: &rgpos::ValueRecord,
    graph: &Graph,
    offsets: &[OffsetRecord],
    // gets incremented every time we see a device offset
    next_offset_idx: &mut usize,
    seen: &mut HashSet<ObjectId>,
) -> usize {
    let subtables = [
        record.x_placement_device.get(),
        record.y_placement_device.get(),
        record.x_advance_device.get(),
        record.y_advance_device.get(),
    ];
    subtables
        .iter()
        .filter_map(|offset| (!offset.is_null()).then_some(*offset.offset()))
        .map(|_| {
            let obj = offsets[*next_offset_idx].object;
            *next_offset_idx += 1;
            if seen.insert(obj) {
                graph.objects[&obj].bytes.len()
            } else {
                0
            }
        })
        .sum()
}

#[cfg(test)]
mod tests {
    use std::collections::BTreeMap;

    use read_fonts::{
        tables::{
            gpos::{PositionSubtables, ValueFormat},
            layout::LookupFlag,
        },
        FontData, FontRead,
    };

    use super::*;
    use crate::{
        tables::{
            gpos::{
                Class1Record, Class2Record, PairPos, PairSet, PairValueRecord, PositionLookup,
                ValueRecord,
            },
            layout::{
                builders::CoverageTableBuilder, Device, DeviceOrVariationIndex, VariationIndex,
            },
        },
        FontWrite, TableWriter,
    };

    // a big empty smoke test that constructs a real table and splits it
    #[test]
    fn split_pair_pos1() {
        let _ = env_logger::builder().is_test(true).try_init();

        struct KernPair(GlyphId16, GlyphId16, i16);
        fn make_pair_pos(pairs: Vec<KernPair>) -> PairPos {
            let mut records = BTreeMap::new();
            for KernPair(one, two, kern) in pairs {
                let value_record1 = ValueRecord::new()
                    .with_x_advance(kern)
                    .with_x_placement(kern)
                    .with_y_advance(kern)
                    .with_y_placement(kern);
                records
                    .entry(one)
                    .or_insert_with(PairSet::default)
                    .pair_value_records
                    .push(PairValueRecord {
                        second_glyph: two,
                        value_record1,
                        value_record2: ValueRecord::default(),
                    })
            }

            let coverage: CoverageTableBuilder = records.keys().copied().collect();
            let pair_sets = records.into_values().collect();

            PairPos::format_1(coverage.build(), pair_sets)
        }

        const N_GLYPHS: u16 = 1500; // manually determined to cause overflow

        let mut pairs = Vec::new();
        for (advance, g1) in (0u16..N_GLYPHS).enumerate() {
            pairs.push(KernPair(
                GlyphId16::new(g1),
                GlyphId16::new(5),
                advance as _,
            ));
            pairs.push(KernPair(
                GlyphId16::new(g1),
                GlyphId16::new(6),
                advance as _,
            ));
            pairs.push(KernPair(
                GlyphId16::new(g1),
                GlyphId16::new(7),
                advance as _,
            ));
            pairs.push(KernPair(
                GlyphId16::new(g1),
                GlyphId16::new(8),
                advance as _,
            ));
        }

        let table = make_pair_pos(pairs);
        let lookup = wlayout::Lookup::new(LookupFlag::empty(), vec![table]);
        let mut graph = TableWriter::make_graph(&lookup);

        let id = graph.root;
        split_pair_pos(&mut graph, id);
        graph.remove_orphans();
        assert!(graph.basic_sort());

        let bytes = graph.serialize();

        let lookup = rlayout::Lookup::<rgpos::PairPosFormat1>::read(FontData::new(&bytes)).unwrap();
        assert_eq!(lookup.sub_table_count(), 2);
        let sub1 = lookup.subtables().get(0).unwrap();
        let sub2 = lookup.subtables().get(1).unwrap();

        // ensure that the split coverage tables equal the unsplit coverage table
        let gids = sub1
            .coverage()
            .unwrap()
            .iter()
            .chain(sub2.coverage().unwrap().iter())
            .map(GlyphId16::to_u16)
            .collect::<Vec<_>>();
        assert_eq!(gids.len(), N_GLYPHS as _);

        let expected = std::iter::successors(Some(0), |n| Some(n + 1))
            .take(N_GLYPHS as _)
            .collect::<Vec<_>>();
        assert_eq!(gids, expected);

        // ensure that the PairSet tables at the split boundaries are as expected
        assert_eq!(sub1.pair_set_count() + sub2.pair_set_count(), N_GLYPHS);
    }

    fn make_pairpos_f1_with_device_tables(g1_count: u16, g2_count: u16) -> PairPos {
        let mut pairsets = Vec::new();
        for g1 in 1u16..=g1_count {
            let records = (1..=g2_count)
                .map(|gid2| {
                    let val = g2_count * g1 + gid2;
                    let valrec = ValueRecord::new()
                        .with_x_advance(val as _)
                        .with_y_advance(val as _)
                        .with_x_placement(val as _)
                        .with_y_advance_device(VariationIndex::new(val, val + 1))
                        .with_x_advance_device(VariationIndex::new(val, val));
                    let valrec2 = valrec
                        .clone()
                        .with_y_advance_device(VariationIndex::new(
                            u16::MAX - val,
                            u16::MAX - val - 1,
                        ))
                        .with_x_advance_device(VariationIndex::new(u16::MAX - val, u16::MAX - val));
                    PairValueRecord::new(GlyphId16::new(gid2), valrec, valrec2)
                })
                .collect();
            pairsets.push(PairSet::new(records));
        }

        let coverage = (1u16..=g1_count).map(GlyphId16::new).collect();
        PairPos::format_1(coverage, pairsets)
    }

    #[test]
    fn split_pairpos1_with_device_tables() {
        let _ = env_logger::builder().is_test(true).try_init();
        // construct a pp1 table that only requires splitting if you accounted
        // for device tables. so:

        // use device format with 5 fields, == 10 bytes per valuerecord,
        // 22 bytes per pairvaluerecord (gid + 2 * value record)
        // let's have two varidx tables per valuerecord, so...
        // 24 bytes of subtables (4 * 6)
        // let's say 100 pair value records per pairset, so:
        // pairset = (2 + 100 * 22) + (100 * 24) == 4602 bytes
        // so 15 pairsets (69030) puts us over the limit.
        const G1_COUNT: u16 = 15;
        const G2_COUNT: u16 = 100;

        // first just naively check that the split function, called directly,
        // works as expected

        let table = make_pairpos_f1_with_device_tables(G1_COUNT, G2_COUNT);
        let lookup = wlayout::Lookup::new(LookupFlag::empty(), vec![table]);
        let mut graph = TableWriter::make_graph(&lookup);

        assert!(lookup.table_type().is_splittable());
        let id = graph.root;
        split_pair_pos(&mut graph, id);
        graph.remove_orphans();
        assert!(graph.basic_sort());

        let bytes = graph.serialize();

        let rlookup =
            rlayout::Lookup::<rgpos::PairPosFormat1>::read(FontData::new(&bytes)).unwrap();
        assert_eq!(rlookup.sub_table_count(), 2);
    }

    #[test]
    fn fully_pack_pairpos1_with_device_tables() {
        // because we're good at packing, we need to include more tables in order
        // to trigger the splitting code, since if naive sorting succeeds we don't
        // bother. 28 PairSet tables is experimentally selected, requiring one split
        const G1_COUNT: u16 = 28;
        const G2_COUNT: u16 = 100;

        let table = make_pairpos_f1_with_device_tables(G1_COUNT, G2_COUNT);
        let lookup = wlayout::Lookup::new(LookupFlag::empty(), vec![table]);
        let lookuplist = wlayout::LookupList::new(vec![lookup]);
        assert!(crate::dump_table(&lookuplist).is_ok());
    }

    #[test]
    fn count_glyph_ranges() {
        fn make_input(glyphs: &[u16]) -> BTreeSet<GlyphId16> {
            glyphs.iter().copied().map(GlyphId16::new).collect()
        }

        assert_eq!(count_num_ranges(&make_input(&[])), 0);
        assert_eq!(count_num_ranges(&make_input(&[1])), 1);
        assert_eq!(count_num_ranges(&make_input(&[1, 2, 3])), 1);
        assert_eq!(count_num_ranges(&make_input(&[1, 2, 3])), 1);
        assert_eq!(count_num_ranges(&make_input(&[1, 2, 3, 5])), 2);
        assert_eq!(count_num_ranges(&make_input(&[1, 2, 3, 5, 6, 7, 10])), 3);
    }

    fn dummy_class_def(
        n_classes: u16,
        n_glyphs_per_class: u16,
        first_gid: u16,
    ) -> wlayout::ClassDef {
        let n_glyphs = n_classes * n_glyphs_per_class;
        (first_gid..first_gid + n_glyphs)
            .map(|gid| {
                let class = (gid - 1) / n_glyphs_per_class;
                (GlyphId16::new(gid), class)
            })
            .collect()
    }

    fn make_pairpos2() -> PositionLookup {
        fn next_class2_rec(i: usize) -> Class2Record {
            // idk how better to cast bits directly
            let val = i16::from_be_bytes(((i % u16::MAX as usize) as u16).to_be_bytes());
            // we add a device table every twelve records, arbitrary, we
            // want some but not a ton
            let value_format = ValueFormat::X_ADVANCE
                | ValueFormat::Y_ADVANCE
                | ValueFormat::X_PLACEMENT
                | ValueFormat::Y_PLACEMENT
                | ValueFormat::Y_ADVANCE_DEVICE;
            let add_device = i % 500 == 0;
            let mut record = ValueRecord::new()
                .with_explicit_value_format(value_format)
                .with_x_advance(val)
                .with_y_advance(val)
                .with_x_placement(val)
                .with_y_placement(val);
            if add_device {
                record = record.with_y_advance_device(DeviceOrVariationIndex::variation_index(
                    0xde, val as u16,
                ));
            }

            Class2Record {
                value_record1: record.clone(),
                value_record2: record,
            }
        }

        const CLASS1_COUNT: u16 = 100;
        const CLASS2_COUNT: u16 = 100;

        let class_def1 = dummy_class_def(CLASS1_COUNT, 4, 1);
        let class_def2 = dummy_class_def(CLASS2_COUNT, 3, 1);

        assert_eq!(class_def1.class_count(), CLASS1_COUNT);
        assert_eq!(class_def2.class_count(), CLASS2_COUNT);
        let coverage = class_def1
            .iter()
            .map(|(gid, _)| gid)
            .collect::<wlayout::CoverageTable>();

        let class1recs = (0..CLASS1_COUNT)
            .map(|i| {
                Class1Record::new(
                    (0..CLASS2_COUNT)
                        .map(|j| next_class2_rec(i as usize * CLASS1_COUNT as usize + j as usize))
                        .collect(),
                )
            })
            .collect();

        let table = PairPos::format_2(coverage, class_def1, class_def2, class1recs);

        let lookup = wlayout::Lookup::new(LookupFlag::empty(), vec![table]);
        PositionLookup::Pair(lookup)
    }

    #[test]
    fn split_pairpos_f2() {
        let _ = env_logger::builder().is_test(true).try_init();
        // okay so... I want a big pairpos format 2 table.
        // this means, mainly, that I want lots of different classes.

        let lookup = make_pairpos2();
        let lookup_list = wlayout::LookupList::new(vec![lookup]);
        let mut graph = TableWriter::make_graph(&lookup_list);

        graph.basic_sort();
        //graph.write_graph_viz("pairpos-test-0.dot").unwrap();
        assert!(graph.pack_objects());
        //graph.write_graph_viz("pairpos-test-1.dot").unwrap();
    }

    #[test]
    fn ensure_split_pairpos_f2_works() {
        let _ = env_logger::builder().is_test(true).try_init();

        // and sanity check that we have the same number of records:
        let lookup = make_pairpos2();
        let expected_n_c2_recs = match &lookup {
            PositionLookup::Pair(pairpos) => pairpos
                .subtables
                .iter()
                .map(|sub| match sub.as_ref() {
                    PairPos::Format1(_) => 0,
                    PairPos::Format2(sub) => sub
                        .class1_records
                        .iter()
                        .map(|c1rec| c1rec.class2_records.len())
                        .sum::<usize>(),
                })
                .sum::<usize>(),
            _ => panic!("wrong lookup type"),
        };
        let lookup_list = wlayout::LookupList::new(vec![lookup]);
        let bytes = crate::dump_table(&lookup_list).unwrap();

        let rlookuplist = rgpos::PositionLookupList::read(FontData::new(&bytes)).unwrap();
        assert_eq!(rlookuplist.lookup_count(), 1);
        let rlookup = rlookuplist.lookups().get(0).unwrap();
        let subtables = match rlookup.subtables().unwrap() {
            PositionSubtables::Pair(subs) => subs.iter().map(|sub| match sub.unwrap() {
                rgpos::PairPos::Format2(sub) => sub,
                rgpos::PairPos::Format1(_) => panic!("wrong subtable type"),
            }),
            _ => panic!("wrong lookup type"),
        };
        let total_c2recs: usize = subtables
            .map(|sub| {
                sub.class1_records()
                    .iter()
                    .map(|c1rec| c1rec.unwrap().class2_records.len())
                    .sum::<usize>()
            })
            .sum();
        assert_eq!(total_c2recs, expected_n_c2_recs);
    }

    #[test]
    fn size_of_value_record_children_sanity() {
        // let's have  single class1class, and three class2 classes
        // we want a duplicate varidx, a null varidx, and a device table?

        fn val_record_with_xadv(x_advance: i16) -> ValueRecord {
            let format = ValueFormat::X_ADVANCE | ValueFormat::X_ADVANCE_DEVICE;
            ValueRecord::new()
                .with_explicit_value_format(format)
                .with_x_advance(x_advance)
        }

        // number of classes, number of glyphs per class, GID of first glyph
        let class_def1 = dummy_class_def(1, 4, 1);
        let class_def2 = dummy_class_def(3, 3, 1);
        let coverage = class_def1.iter().map(|(gid, _)| gid).collect();
        let actual_device_table = Device::new(12, 15, &[118, 119, 127, 99]);
        // sanity check the size of the device table, these are weird:
        assert_eq!(crate::dump_table(&actual_device_table).unwrap().len(), 10);
        let class1_records = vec![Class1Record::new(vec![
            Class2Record::new(
                val_record_with_xadv(5),
                val_record_with_xadv(6)
                    .with_x_advance_device(DeviceOrVariationIndex::variation_index(4, 20)),
            ),
            Class2Record::new(
                val_record_with_xadv(7)
                    .with_x_advance_device(DeviceOrVariationIndex::Device(actual_device_table)),
                // a duplicate table
                val_record_with_xadv(8)
                    .with_x_advance_device(DeviceOrVariationIndex::variation_index(4, 20)),
            ),
            Class2Record::new(
                val_record_with_xadv(9)
                    .with_x_advance_device(DeviceOrVariationIndex::variation_index(6, 9)),
                val_record_with_xadv(10),
            ),
        ])];
        let ppf2 = PairPos::format_2(coverage, class_def1, class_def2, class1_records);

        // now we need to pretend we're in the split_pair_pos_format_2 fn
        let mut graph = TableWriter::make_graph(&ppf2);
        assert!(graph.pack_objects());
        let root_id = graph.root;
        let ppf2_data = &graph.objects[&root_id];
        let ppf2 = ppf2_data.reparse::<rgpos::PairPosFormat2>().unwrap();
        assert_eq!(ppf2.class1_records().len(), 1);
        let c1rec = ppf2.class1_records().get(0).unwrap();
        let mut visited = HashSet::new();
        let mut next_device_offset = 3;
        assert_eq!(c1rec.class2_records.len(), 3);

        // a little helper so we don't have to have this huge fn call in each assert
        let mut children_size = |record: &rgpos::ValueRecord| -> usize {
            size_of_value_record_children(
                record,
                &graph,
                &ppf2_data.offsets,
                &mut next_device_offset,
                &mut visited,
            )
        };

        let c2rec1 = c1rec.class2_records().get(0).unwrap();
        assert_eq!(children_size(c2rec1.value_record1()), 0, "no subtables");
        assert_eq!(children_size(c2rec1.value_record2()), 6, "one new varidx");
        let c2rec2 = c1rec.class2_records().get(1).unwrap();
        assert_eq!(children_size(c2rec2.value_record1()), 10, "a device table");
        assert_eq!(children_size(c2rec2.value_record2()), 0, "duplicate table");
        let c2rec3 = c1rec.class2_records().get(2).unwrap();
        assert_eq!(children_size(c2rec3.value_record1()), 6, "new varidx table");
        assert_eq!(children_size(c2rec3.value_record2()), 0, "a null offset");
        assert_eq!(next_device_offset, 7, "we visited all offsets");
    }
}
