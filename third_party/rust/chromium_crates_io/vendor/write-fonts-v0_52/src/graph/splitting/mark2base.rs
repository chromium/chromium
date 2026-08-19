//! splitting of MarkToBase positioning subtables

use std::collections::HashSet;

use font_types::{FixedSize, Offset16};
use read_fonts::tables::{gpos as rgpos, layout as rlayout};

use crate::{tables::layout::CoverageTable, write::TableData};

use super::{Graph, ObjectId};

pub(crate) fn split_mark_to_base(graph: &mut Graph, lookup: ObjectId) {
    super::split_subtables(graph, lookup, split_mark_to_base_subtable)
}

// based off of <https://github.com/harfbuzz/harfbuzz/blob/f26fd69d858642d764/src/graph/markbasepos-graph.hh#L212>
fn split_mark_to_base_subtable(graph: &mut Graph, subtable: ObjectId) -> Option<Vec<ObjectId>> {
    // the base size for the table, not the size of the 'base glyphs subtable'
    const BASE_SIZE: usize = 6 * u16::RAW_BYTE_LEN // six short fields in subtable
                               + u16::RAW_BYTE_LEN // empty mark array table
                               + u16::RAW_BYTE_LEN; // empty base array table
    let data = &graph.objects[&subtable];
    let base_coverage_id = data.offsets[1].object;
    let base_coverage_size = graph.objects[&base_coverage_id].bytes.len();
    debug_assert!(data.reparse::<rgpos::MarkBasePosFormat1>().is_ok());

    let min_subtable_size = BASE_SIZE + base_coverage_size;
    let class_info = get_class_info(graph, subtable);

    let base_array_off = &data.offsets[3];
    let base_array_data = &graph.objects[&base_array_off.object];

    let mark_class_count: u16 = data.read_at(6).unwrap_or(0);
    debug_assert_eq!(class_info.len(), mark_class_count as usize);
    let base_count: u16 = base_array_data.read_at(0).unwrap_or(0);

    let mut partial_coverage_size = 4;
    let mut accumulated = min_subtable_size;
    let mut split_points = Vec::new();
    let mut visited = HashSet::new();

    for i in 0..mark_class_count {
        let info = &class_info[i as usize];
        partial_coverage_size += u16::RAW_BYTE_LEN * info.marks.len();

        let mut accumulated_delta =
            // the records for the marks in this class
            rgpos::MarkRecord::RAW_BYTE_LEN * info.marks.len()
            // plus an offset in each base record for this class
            + Offset16::RAW_BYTE_LEN * base_count as usize;
        accumulated_delta += compute_subgraph_size(&info.children, graph, &mut visited);
        accumulated += accumulated_delta;
        let total = accumulated + partial_coverage_size;

        if total > super::MAX_TABLE_SIZE {
            log::trace!("adding split at {i}");
            split_points.push(i as usize);
            accumulated = min_subtable_size + accumulated_delta;
            partial_coverage_size = 4 + u16::RAW_BYTE_LEN * info.marks.len();
            visited.clear();
        }
    }

    log::debug!(
        "nothing to split, size '{}'",
        accumulated + partial_coverage_size
    );

    if split_points.is_empty() {
        return None;
    }

    split_points.push(mark_class_count as _);
    let mut new_subtables = Vec::new();
    let mut prev_split = 0;

    for next_split in split_points {
        let new_subtable = split_off_mark_pos(graph, subtable, prev_split, next_split, &class_info);
        prev_split = next_split;
        new_subtables.push(graph.add_object(new_subtable));
    }

    Some(new_subtables)
}

// based off of <https://github.com/harfbuzz/harfbuzz/blob/f26fd69d858642/src/graph/markbasepos-graph.hh#L411>
fn split_off_mark_pos(
    graph: &mut Graph,
    subtable: ObjectId,
    start: usize,
    end: usize,
    class_info: &[Mark2BaseClassInfo],
) -> TableData {
    let mark_coverage_id = graph.objects[&subtable].offsets.first().unwrap().object;
    let mark_coverage = &graph.objects[&mark_coverage_id];
    let mark_coverage = mark_coverage.reparse::<rlayout::CoverageTable>().unwrap();
    let data = &graph.objects[&subtable];
    let base_coverage_id = data.offsets[1].object;
    let mark_array_id = data.offsets[2].object;
    let base_array_id = data.offsets[3].object;
    let old_mark_array = &graph.objects[&mark_array_id];
    let old_base_array = &graph.objects[&base_array_id];
    let mark_class_count = (end - start) as u16;
    let mut new_subtable = TableData::new(data.type_);

    let mark_glyphs_by_cov_id: HashSet<_> = (start..end)
        .flat_map(|class_idx| class_info[class_idx].marks.iter())
        .copied()
        .collect();
    let new_mark_coverage: CoverageTable = mark_coverage
        .iter()
        .enumerate()
        .filter_map(|(i, gid)| mark_glyphs_by_cov_id.contains(&i).then_some(gid))
        .collect();
    let new_mark_coverage = super::make_table_data(&new_mark_coverage);
    let new_mark_array = split_off_mark_array(old_mark_array, start as u16, &mark_glyphs_by_cov_id);
    let new_base_array = split_off_base_array(old_base_array, start, end, class_info.len());
    let new_mark_coverage_id = graph.add_object(new_mark_coverage);
    let new_mark_array_id = graph.add_object(new_mark_array);
    let new_base_array_id = graph.add_object(new_base_array);

    new_subtable.write(1u16); // format
    new_subtable.add_offset(new_mark_coverage_id, 2, 0);
    new_subtable.add_offset(base_coverage_id, 2, 0);
    new_subtable.write(mark_class_count);
    new_subtable.add_offset(new_mark_array_id, 2, 0);
    new_subtable.add_offset(new_base_array_id, 2, 0);

    new_subtable
}

// <https://github.com/harfbuzz/harfbuzz/blob/f26fd69d858642d76413b8f/src/graph/markbasepos-graph.hh#L170>
fn split_off_mark_array(
    old_mark_array: &TableData,
    first_class: u16,
    mark_glyph_coverage_ids: &HashSet<usize>,
) -> TableData {
    let mark_array = old_mark_array.reparse::<rgpos::MarkArray>().unwrap();
    let mark_count = mark_glyph_coverage_ids.len() as u16;

    let mut new_mark_array = TableData::new(old_mark_array.type_);
    new_mark_array.write(mark_count);

    for (i, mark_record) in mark_array.mark_records().iter().enumerate() {
        if !mark_glyph_coverage_ids.contains(&i) {
            continue;
        }
        let new_class = mark_record.mark_class() - first_class;
        let anchor_offset = old_mark_array.offsets[i].object;

        new_mark_array.write(new_class);
        new_mark_array.add_offset(anchor_offset, 2, 0);
    }

    new_mark_array
}

fn split_off_base_array(
    old_base_array: &TableData,
    start: usize,
    end: usize,
    old_mark_class_count: usize,
) -> TableData {
    let mut new_base_array = TableData::new(old_base_array.type_);
    let base_count: u16 = old_base_array.read_at(0).unwrap_or(0);
    new_base_array.write(base_count);

    // the base array contains a (base_count x mark_count) matrix of offsets.
    // for each base, we want to prune the marks to only include those
    // in the range (start..end).

    let base_array: rgpos::BaseArray = old_base_array
        .reparse_with_args(old_mark_class_count as u16)
        .unwrap();

    let mut next_offset_idx = 0;
    // because offsets may be null, and there is no pattern, we visit each one
    for base_record in base_array.base_records().iter() {
        let base_record = base_record.unwrap();
        for (mark_class, offset) in base_record.base_anchor_offsets().iter().enumerate() {
            let has_offset = !offset.get().is_null();
            let in_range = (start..end).contains(&mark_class);

            if in_range {
                if has_offset {
                    let id = old_base_array.offsets[next_offset_idx].object;
                    new_base_array.add_offset(id, 2, 0);
                } else {
                    // manually write null value
                    new_base_array.write(0u16);
                }
            }

            if has_offset {
                next_offset_idx += 1;
            }
        }
    }

    new_base_array
}

/// Information about a single mark class in a Mark2Base subtable
#[derive(Clone, Debug, Default)]
struct Mark2BaseClassInfo {
    // value is the order in the coverage table
    marks: HashSet<usize>,
    children: Vec<ObjectId>,
}

// this is not general purpose! tailored to mark2pos
fn compute_subgraph_size(
    objects: &[ObjectId],
    graph: &Graph,
    visited: &mut HashSet<ObjectId>,
) -> usize {
    objects
        .iter()
        .map(|id| {
            if !visited.insert(*id) {
                return 0;
            }
            // the size of the anchor table
            let base_size = graph.objects[id].bytes.len();
            // the size of any devices or variation indices.
            let children_size = graph.objects[id]
                .offsets
                .iter()
                .map(|id| {
                    // the mark2pos subgraph is only ever two layers deep
                    debug_assert!(graph.objects[&id.object].offsets.is_empty());
                    if visited.insert(id.object) {
                        graph.objects[&id.object].bytes.len()
                    } else {
                        0
                    }
                })
                .sum::<usize>();
            base_size + children_size
        })
        .sum()
}

// get info about the mark classes in a MarkToBase subtable.
//
// based on <https://github.com/harfbuzz/harfbuzz/blob/f26fd69d858642d76/src/graph/markbasepos-graph.hh#L316>
fn get_class_info(graph: &Graph, subtable: ObjectId) -> Vec<Mark2BaseClassInfo> {
    let data = &graph.objects[&subtable];
    let mark_class_count: u16 = data.read_at(6).unwrap_or(0);
    let mut class_to_info = vec![Mark2BaseClassInfo::default(); mark_class_count as usize];
    let mark_array_off = &data.offsets[2];
    assert_eq!(mark_array_off.pos, 8);
    let mark_array_data = &graph.objects[&mark_array_off.object];
    let mark_array = mark_array_data.reparse::<rgpos::MarkArray>().unwrap();
    // okay so:
    // - there is one mark record for each mark glyph
    // - there may be multiple mark glyphs with the same class
    for (i, mark_record) in mark_array.mark_records().iter().enumerate() {
        let mark_class = mark_record.mark_class();
        // this shouldn't happen unless data is malformed? but harfbuzz includes
        // this check, and it doesn't hurt.
        if mark_class >= mark_class_count {
            continue;
        }
        let anchor_table_id = mark_array_data.offsets[i].object;
        class_to_info[mark_class as usize].marks.insert(i);
        class_to_info[mark_class as usize]
            .children
            .push(anchor_table_id);
    }

    // - base array declares one record for each base glyph (in cov table order)
    // - each record has an anchor for each mark glyph
    let base_array_off = &data.offsets[3];
    assert_eq!(base_array_off.pos, 10);
    let base_array_data = &graph.objects[&base_array_off.object];

    for offsets in base_array_data.offsets.chunks_exact(mark_class_count as _) {
        for (i, off) in offsets.iter().enumerate() {
            class_to_info[i].children.push(off.object)
        }
    }

    class_to_info
}

#[cfg(test)]
mod tests {
    use std::ops::Range;

    use font_types::GlyphId16;
    use read_fonts::{
        tables::{gpos::PositionSubtables, layout::LookupFlag},
        FontRead,
    };

    use crate::{
        tables::{
            gpos::{AnchorTable, BaseArray, BaseRecord, MarkArray, MarkBasePosFormat1, MarkRecord},
            layout::{DeviceOrVariationIndex, Lookup, LookupList, VariationIndex},
        },
        TableWriter,
    };

    use super::*;

    // too fancy, but:
    //
    // we want to create anchor tables for each glyph, and later check that
    // we have the correct anchor tables after splitting.
    //
    // the idea here is to map u16s to i16s in a way that is easy to understand
    // at a glance, so we do 0, 1 .. i16::MAX, -1, -2 .. i16::MIN
    fn u16_to_i16(val: u16) -> i16 {
        match i16::try_from(val) {
            Ok(val) => val,
            Err(_) => i16::MAX.saturating_sub_unsigned(val),
        }
    }

    fn make_mark_array(
        class_count: u16,
        glyphs_per_class: u16,
        // if true we add VariationIndex subtables to our anchors,
        // which is important because it adds an extra layer to the graph
        // and complicates packing
        include_var_index_tables: bool,
    ) -> MarkArray {
        let mark_glyph_count = class_count * glyphs_per_class;
        let records = (0..mark_glyph_count)
            .map(|cov_idx| {
                let mark_class = cov_idx / glyphs_per_class;
                let val = u16_to_i16(cov_idx);
                let anchor = if include_var_index_tables {
                    let var_idx = VariationIndex::new(cov_idx, cov_idx);
                    AnchorTable::format_3(val, val, Some(var_idx.into()), None)
                } else {
                    AnchorTable::format_1(val, val)
                };
                MarkRecord::new(mark_class, anchor)
            })
            .collect();
        MarkArray::new(records)
    }

    fn make_base_array(
        base_count: u16,
        mark_class_count: u16,
        include_var_index_tables: bool,
    ) -> BaseArray {
        let base_records = (0..base_count)
            .map(|i| {
                let mark_anchors = (0..mark_class_count)
                    .map(|j| {
                        // even anchors exist, odd anchors are null
                        (j % 2 == 0).then(|| {
                            let val = u16_to_i16(i * mark_class_count + j);
                            if include_var_index_tables {
                                // we want duplicate variation index tables, since it
                                // poses different challenges when resolving offsets
                                let var_idx = VariationIndex::new(j, j);
                                AnchorTable::format_3(val, val, Some(var_idx.into()), None)
                            } else {
                                AnchorTable::format_1(val, val)
                            }
                        })
                    })
                    .collect();
                BaseRecord::new(mark_anchors)
            })
            .collect();
        BaseArray::new(base_records)
    }

    // big sanity check of splitting a real table
    #[test]
    fn split_mark_2_base() {
        let _ = env_logger::builder().is_test(true).try_init();
        const MARK_CLASS_COUNT: u16 = 100;
        const MARKS_PER_CLASS: u16 = 4;
        const N_BASES: u16 = 100;
        const N_MARKS: u16 = MARK_CLASS_COUNT * MARKS_PER_CLASS;
        const FIRST_BASE_GLYPH: u16 = 2;
        const FIRST_MARK_GLYPH: u16 = 2000;

        let mark_coverage = (FIRST_MARK_GLYPH..FIRST_MARK_GLYPH + N_MARKS)
            .map(GlyphId16::new)
            .collect();
        let base_coverage = (FIRST_BASE_GLYPH..FIRST_BASE_GLYPH + N_BASES)
            .map(GlyphId16::new)
            .collect();
        let mark_array = make_mark_array(MARK_CLASS_COUNT, MARKS_PER_CLASS, false);
        let base_array = make_base_array(N_BASES, MARK_CLASS_COUNT, false);

        let table = MarkBasePosFormat1::new(mark_coverage, base_coverage, mark_array, base_array);
        let lookup = Lookup::new(LookupFlag::empty(), vec![table]);
        let mut graph = TableWriter::make_graph(&lookup);
        let id = graph.root;
        assert!(graph.objects[&id].type_.is_promotable());
        split_mark_to_base(&mut graph, id);
        graph.remove_orphans();
        assert!(graph.basic_sort());

        let dumped = graph.serialize();
        let read_back =
            rlayout::Lookup::<rgpos::MarkBasePosFormat1>::read(dumped.as_slice().into()).unwrap();

        // quick sanity check: do the coverage tables match?
        let mark_cov: CoverageTable = read_back
            .subtables()
            .iter()
            .flat_map(|sub| {
                sub.ok()
                    .and_then(|sub| sub.mark_coverage().ok().map(|cov| cov.iter()))
            })
            .flatten()
            .collect();
        assert_eq!(&mark_cov, lookup.subtables[0].mark_coverage.as_ref());
        let base_cov: CoverageTable = read_back
            .subtables()
            .iter()
            .flat_map(|sub| {
                sub.ok()
                    .and_then(|sub| sub.base_coverage().ok().map(|cov| cov.iter()))
            })
            .flatten()
            .collect();
        assert_eq!(&base_cov, lookup.subtables[0].base_coverage.as_ref());

        // this is a closure for comparing the pre-and-post split values
        let compare_old_and_new = |base_gid, mark_gid| {
            // now let's manually check one of the records.
            let base_gid = GlyphId16::new(base_gid);
            let mark_gid = GlyphId16::new(mark_gid);

            // find the values in the original table:
            let old_subtable = &lookup.subtables[0];

            let base_cov_idx = base_gid.to_u16() - FIRST_BASE_GLYPH;
            let mark_cov_idx = mark_gid.to_u16() - FIRST_MARK_GLYPH;

            // first find the original values
            let orig_mark_record = &old_subtable.mark_array.mark_records[mark_cov_idx as usize];
            let orig_base_anchor = &old_subtable.base_array.base_records[base_cov_idx as usize]
                .base_anchors[orig_mark_record.mark_class as usize];

            // then find the post-split subtable with this mark glyph
            let new_subtable = read_back
                .subtables()
                .iter()
                .find_map(|sub| {
                    let sub = sub.unwrap();
                    sub.mark_coverage()
                        .unwrap()
                        .get(mark_gid)
                        .is_some()
                        .then_some(sub)
                })
                .unwrap();
            let new_mark_idx = new_subtable.mark_coverage().unwrap().get(mark_gid).unwrap();
            let new_base_idx = new_subtable.base_coverage().unwrap().get(base_gid).unwrap();
            let new_mark_array = new_subtable.mark_array().unwrap();
            let new_mark_record = &new_mark_array.mark_records()[new_mark_idx as usize];
            let new_mark_anchor = new_mark_record
                .mark_anchor(new_mark_array.offset_data())
                .unwrap();
            let new_base_array = new_subtable.base_array().unwrap();
            let new_base_anchor = new_base_array
                .base_records()
                .get(new_base_idx as usize)
                .unwrap()
                .base_anchors(new_base_array.offset_data())
                .get(new_mark_record.mark_class() as usize)
                .transpose()
                .unwrap();

            fn get_f1_anchor_x_coords(old: &AnchorTable, new: &rgpos::AnchorTable) -> (i16, i16) {
                match (old, new) {
                    (AnchorTable::Format1(old), rgpos::AnchorTable::Format1(new)) => {
                        (old.x_coordinate, new.x_coordinate())
                    }
                    _ => panic!("only format 1 here"),
                }
            }

            let (old_mark_x, new_mark_x) =
                get_f1_anchor_x_coords(orig_mark_record.mark_anchor.as_ref(), &new_mark_anchor);
            assert_eq!(old_mark_x, new_mark_x);
            let (old_base_x, new_base_x) = orig_base_anchor
                .as_ref()
                .zip(new_base_anchor.as_ref())
                .map(|(old, new)| get_f1_anchor_x_coords(old, new))
                .unwrap_or_default();
            assert_eq!(old_base_x, new_base_x);
        };

        // the first two, the last two, and an even/odd pair in the middle
        for base in [2, 3, 50, 51, 100, 101] {
            for mark in [2000, 2001, 2222, 2211, 2398, 2399] {
                compare_old_and_new(base, mark);
            }
        }
    }

    #[test]
    fn full_split_including_variation_index_tables() {
        let _ = env_logger::builder().is_test(true).try_init();

        const MARK_CLASS_COUNT: u16 = 120;
        const MARKS_PER_CLASS: u16 = 4;
        const N_BASES: u16 = 500;
        const N_MARKS: u16 = MARK_CLASS_COUNT * MARKS_PER_CLASS;
        const FIRST_BASE_GLYPH: u16 = 2;
        const FIRST_MARK_GLYPH: u16 = 2000;

        let mark_coverage = (FIRST_MARK_GLYPH..FIRST_MARK_GLYPH + N_MARKS)
            .map(GlyphId16::new)
            .collect();
        let base_coverage = (FIRST_BASE_GLYPH..FIRST_BASE_GLYPH + N_BASES)
            .map(GlyphId16::new)
            .collect();
        let mark_array = make_mark_array(MARK_CLASS_COUNT, MARKS_PER_CLASS, true);
        let base_array = make_base_array(N_BASES, MARK_CLASS_COUNT, true);

        let table = MarkBasePosFormat1::new(mark_coverage, base_coverage, mark_array, base_array);
        let lookup = Lookup::new(LookupFlag::empty(), vec![table]);
        let lookup_list = LookupList::new(vec![lookup]);
        let bytes = crate::dump_table(&lookup_list).unwrap();
        let read_back = rgpos::PositionLookupList::read(bytes.as_slice().into()).unwrap();
        assert_eq!(read_back.lookup_count(), 1);
        let lookup = read_back.lookups().get(0).unwrap();

        let subtables: Vec<_> = match lookup.subtables().unwrap() {
            PositionSubtables::MarkToBase(subs) => subs.iter().map(|sub| sub.unwrap()).collect(),
            _ => panic!("wrong lookup type"),
        };

        assert_eq!(subtables.len(), 7);

        // now we want to check to see that one of our variation index tables is correct.
        // in the markarray, each mark has a varidx table, with both values equal
        // to that mark's original coverage index.

        let mark_cov_idx_to_test = 72;
        let orig_anchor = &lookup_list.lookups[0].subtables[0].mark_array.mark_records
            [mark_cov_idx_to_test]
            .mark_anchor;
        // sanity check that our logic makes sense
        match orig_anchor.as_ref() {
            AnchorTable::Format3(anchor) => match anchor.x_device.as_ref() {
                Some(DeviceOrVariationIndex::VariationIndex(varidx))
                    if varidx.delta_set_outer_index == mark_cov_idx_to_test as u16 => {}
                _ => panic!("bad assumptions"),
            },
            _ => panic!("very bad assumptions"),
        };

        let gid = GlyphId16::new(FIRST_MARK_GLYPH + mark_cov_idx_to_test as u16);
        let subtable = subtables
            .iter()
            .find(|sub| sub.mark_coverage().unwrap().get(gid).is_some())
            .expect("should exist in some subtable");
        let new_cov_idx = subtable.mark_coverage().unwrap().get(gid).unwrap();
        let mark_array = subtable.mark_array().unwrap();
        let mark_record = &mark_array.mark_records()[new_cov_idx as usize];
        let mark_anchor = mark_record.mark_anchor(mark_array.offset_data()).unwrap();
        let rgpos::AnchorTable::Format3(mark_anchor) = mark_anchor else {
            panic!("wrong format")
        };

        // moment of truth: did we preserve the correct var index table in
        // this mark's anchor?
        assert!(
            matches!(mark_anchor.x_device().transpose().unwrap(), Some(rgpos::DeviceOrVariationIndex::VariationIndex(varidx)) if varidx.delta_set_outer_index() == mark_cov_idx_to_test as u16)
        );
    }

    #[test]
    fn test_my_test_helper() {
        assert_eq!(u16_to_i16(1), 1);
        assert_eq!(u16_to_i16(32767), 32767);
        assert_eq!(u16_to_i16(32768), -1);
        assert_eq!(u16_to_i16(u16::MAX), i16::MIN);
    }

    #[test]
    fn split_mark_array() {
        const N_GLYPHS: u16 = 900;
        const N_CLASSES: u16 = 75;
        const GLYPHS_PER_CLASS: u16 = N_GLYPHS / N_CLASSES;
        const SPLIT_CLASS_RANGE: Range<u16> = 20..25;

        let mark_array = make_mark_array(N_CLASSES, GLYPHS_PER_CLASS, false);

        let graph = TableWriter::make_graph(&mark_array);
        let data = &graph.objects[&graph.root];
        // now let's imagine we're splitting off classes 20..25
        let mark_glyph_coverage_ids = mark_array
            .mark_records
            .iter()
            .enumerate()
            .filter_map(|(i, rec)| SPLIT_CLASS_RANGE.contains(&rec.mark_class).then_some(i))
            .collect::<HashSet<_>>();

        assert_eq!(
            mark_glyph_coverage_ids.len(),
            SPLIT_CLASS_RANGE.len() * GLYPHS_PER_CLASS as usize
        );

        let result = split_off_mark_array(data, SPLIT_CLASS_RANGE.start, &mark_glyph_coverage_ids);
        assert_eq!(
            result.offsets.len(),
            GLYPHS_PER_CLASS as usize * SPLIT_CLASS_RANGE.len()
        );

        let reparsed = result.reparse::<rgpos::MarkArray>().unwrap();
        // ensure classes are correct
        for (i, rec) in reparsed.mark_records().iter().enumerate() {
            let exp_class = i as u16 / GLYPHS_PER_CLASS;
            assert_eq!(rec.mark_class(), exp_class);
        }

        // ensure anchors are right
        for (i, offset) in result.offsets.iter().enumerate() {
            let gid_delta = SPLIT_CLASS_RANGE.start * GLYPHS_PER_CLASS;
            let gid = i as u16 + gid_delta;
            let anchor_val = u16_to_i16(gid);
            let anchor_table = &graph.objects[&offset.object];
            let anchor_table = anchor_table.reparse::<rgpos::AnchorFormat1>().unwrap();
            assert_eq!(anchor_table.x_coordinate(), anchor_val);
        }
    }

    #[test]
    fn split_base_array() {
        const N_CLASSES: u16 = 20;
        const N_BASES: u16 = 10;
        const SPLIT_CLASS_RANGE: Range<u16> = 15..20;

        let base_array = make_base_array(N_BASES, N_CLASSES, false);
        let graph = TableWriter::make_graph(&base_array);
        let data = &graph.objects[&graph.root];

        let result = split_off_base_array(
            data,
            SPLIT_CLASS_RANGE.start as _,
            SPLIT_CLASS_RANGE.end as _,
            N_CLASSES as _,
        );

        assert_eq!(result.read_at::<u16>(0).unwrap(), N_BASES);
        let mut idx = 0;
        for base in 0..N_BASES {
            for mark_class in 0..SPLIT_CLASS_RANGE.len() as u16 {
                let original_mark_class = mark_class + SPLIT_CLASS_RANGE.start;
                if original_mark_class % 2 == 0 {
                    // anchor table is non-null
                    let anchor_id = result.offsets[idx].object;
                    let anchor = &graph.objects[&anchor_id];
                    let anchor = anchor.reparse::<rgpos::AnchorFormat1>().unwrap();

                    let exp_val = u16_to_i16((base * N_CLASSES) + original_mark_class);
                    assert_eq!(
                        anchor.x_coordinate(),
                        exp_val,
                        "base {base} mark {mark_class} {anchor_id:?}"
                    );
                    idx += 1;
                }
            }
        }
    }
}
