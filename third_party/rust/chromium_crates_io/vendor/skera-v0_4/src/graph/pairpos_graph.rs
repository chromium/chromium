//! Split  PairPos table in a graph
use crate::{
    graph::{
        coverage_graph::{add_new_coverage, coverage_glyphs, make_coverage},
        layout::DataBytes,
        Graph, RepackError,
    },
    serialize::{Link, LinkWidth, ObjIdx, Serializer},
    Serialize,
};
use std::collections::BTreeMap;
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{
            gpos::ValueFormat,
            layout::{ClassDef, CoverageTable},
        },
        FontData, FontRead,
    },
    types::{FixedSize, GlyphId, Offset16, Scalar},
};

// output only contains new subtable indices
// ref:<https://github.com/harfbuzz/harfbuzz/blob/708bf4a0c80b9f323c9a1c8ec00ff9c2cb429b1f/src/graph/pairpos-graph.hh#L607>
pub(crate) fn split_pairpos(
    graph: &mut Graph,
    table_idx: ObjIdx,
) -> Result<Vec<ObjIdx>, RepackError> {
    let format_bytes = graph
        .vertex_data(table_idx)
        .ok_or(RepackError::GraphErrorInvalidObjIndex)?
        .get(0..2)
        .unwrap();
    let format = u16::read(format_bytes).ok_or(RepackError::ErrorReadTable)?;
    match format {
        1 => split_format1(graph, table_idx),
        2 => split_format2(graph, table_idx),
        _ => Ok(Vec::new()),
    }
}

fn split_format1(graph: &mut Graph, table_idx: ObjIdx) -> Result<Vec<ObjIdx>, RepackError> {
    let coverage_idx = graph
        .index_for_position(table_idx, PairPosFormat1::COVERAGE_OFFSET_POS)
        .ok_or(RepackError::ErrorReadTable)?;
    let all_glyphs = coverage_glyphs(graph, coverage_idx)?;
    let num_pair_sets = PairPosFormat1::from_graph(graph, table_idx)?.num_pair_sets() as usize;
    let split_points = compute_format1_split_points(graph, table_idx, coverage_idx, num_pair_sets)?;
    if split_points.is_empty() {
        return Ok(Vec::new());
    }

    let mut new_table_indices = Vec::new();
    for i in 0..split_points.len() {
        let start = split_points[i];
        let end = if i + 1 < split_points.len() { split_points[i + 1] } else { num_pair_sets };
        let new_idx = clone_range_format1(graph, table_idx, start, end, &all_glyphs)?;
        new_table_indices.push(new_idx);
    }

    shrink_format1(graph, table_idx, coverage_idx, &all_glyphs, split_points[0])?;

    Ok(new_table_indices)
}

fn clone_range_format1(
    graph: &mut Graph,
    table_idx: ObjIdx,
    start: usize,
    end: usize,
    coverage_glyphs: &[GlyphId],
) -> Result<ObjIdx, RepackError> {
    let new_pair_set_count = end - start;
    let new_table_size = PairPosFormat1::MIN_SIZE + new_pair_set_count * Offset16::RAW_BYTE_LEN;
    let new_table_idx = graph.new_vertex(new_table_size)?;

    let new_coverage_idx = graph.new_vertex(0)?;
    make_coverage(graph, new_coverage_idx, coverage_glyphs, start..end)?;

    graph.add_parent_child_link(
        new_table_idx,
        new_coverage_idx,
        LinkWidth::Two,
        PairPosFormat1::COVERAGE_OFFSET_POS,
        false,
    )?;

    // Copy value formats from original table
    let (v_fmt1, v_fmt2) = {
        let old_table = PairPosFormat1::from_graph(graph, table_idx)?;
        (old_table.value_format1(), old_table.value_format2())
    };

    let mut new_table = PairPosFormat1::from_graph(graph, new_table_idx)?;
    new_table.set_format(1);
    new_table.set_value_format1(v_fmt1);
    new_table.set_value_format2(v_fmt2);
    new_table.set_pair_set_count(new_pair_set_count as u16);

    // Move pair sets
    for i in 0..new_pair_set_count {
        let old_pos = PairPosFormat1::PAIR_SET_OFFSETS_START + (start + i) as u32 * 2;
        let new_pos = PairPosFormat1::PAIR_SET_OFFSETS_START + i as u32 * 2;
        graph.move_child(table_idx, old_pos, new_table_idx, new_pos, 2)?;
    }

    Ok(new_table_idx)
}

fn compute_format1_split_points(
    graph: &mut Graph,
    table_idx: ObjIdx,
    coverage_idx: ObjIdx,
    num_pair_sets: usize,
) -> Result<Vec<usize>, RepackError> {
    if num_pair_sets == 0 {
        return Ok(Vec::new());
    }

    let table_links =
        graph.vertex(table_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?.real_links();
    let coverage_table_size =
        graph.vertex(coverage_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?.table_size();

    let mut accumulated = PairPosFormat1::MIN_SIZE;
    let mut out = Vec::new();
    let mut visited = IntSet::empty();

    // Coverage table size estimate
    // For Format 1, it's 4 + 2 * num_glyphs
    // Since we are splitting by PairSet, each new subtable's coverage will have
    // (end - start) glyphs
    let mut partial_coverage_size = 4;

    for i in 0..num_pair_sets {
        let pos =
            PairPosFormat1::PAIR_SET_OFFSETS_START + (i as u32 * Offset16::RAW_BYTE_LEN as u32);
        let Some(pairset_idx) = table_links.get(&pos).map(|l| l.obj_idx()) else {
            continue;
        };

        // Each PairSet adds an offset in the main table (2 bytes) and a glyph in the
        // coverage table (2 bytes)
        partial_coverage_size += Offset16::RAW_BYTE_LEN;

        let pairset_size = graph.find_subgraph_size(pairset_idx, &mut visited, u16::MAX)?;
        accumulated += pairset_size + Offset16::RAW_BYTE_LEN;

        if accumulated + partial_coverage_size.min(coverage_table_size) > u16::MAX as usize {
            out.push(i);
            // Reset for the next subtable
            accumulated = PairPosFormat1::MIN_SIZE + Offset16::RAW_BYTE_LEN + pairset_size;
            partial_coverage_size = 4 + Offset16::RAW_BYTE_LEN;
            visited.clear();
        }
    }

    Ok(out)
}

fn shrink_format1(
    graph: &mut Graph,
    table_idx: ObjIdx,
    coverage_idx: ObjIdx,
    coverage_glyphs: &[GlyphId],
    shrink_point: usize,
) -> Result<(), RepackError> {
    {
        let mut table = PairPosFormat1::from_graph(graph, table_idx)?;
        let old_pair_set_count = table.num_pair_sets() as usize;
        if shrink_point >= old_pair_set_count {
            return Ok(());
        }
        table.set_pair_set_count(shrink_point as u16);
    }

    let table_v = graph.mut_vertex(table_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
    table_v.tail = table_v.head + PairPosFormat1::MIN_SIZE + shrink_point * Offset16::RAW_BYTE_LEN;

    make_coverage(graph, coverage_idx, coverage_glyphs, 0..shrink_point)
}

struct Format2TableInfo {
    table_idx: ObjIdx,
    coverage_idx: ObjIdx,
    value_format1: u16,
    value_format2: u16,
    class_def1_idx: ObjIdx,
    class_def2_idx: ObjIdx,
    class1_count: u16,
    class2_count: u16,
    glyph_classes: Vec<(GlyphId, u16)>,
    total_value_len: u32,
    v1_len: u32,
    class1_record_size: usize,
    format1_device_indices: Vec<u8>,
    format2_device_indices: Vec<u8>,
}

fn split_format2(graph: &mut Graph, table_idx: ObjIdx) -> Result<Vec<ObjIdx>, RepackError> {
    let Some(table_info) = get_table_info(graph, table_idx) else {
        return Ok(Vec::new());
    };

    let split_points = compute_format2_split_points(graph, &table_info)?;
    if split_points.is_empty() {
        return Ok(Vec::new());
    }

    let mut new_table_indices = Vec::new();
    for i in 0..split_points.len() {
        let start = split_points[i];
        let end =
            if i + 1 < split_points.len() { split_points[i + 1] } else { table_info.class1_count };
        let new_idx = clone_range_format2(graph, start, end, &table_info)?;
        new_table_indices.push(new_idx);
    }

    shrink_format2(graph, &table_info, split_points[0])?;
    Ok(new_table_indices)
}

fn get_table_info(graph: &mut Graph, table_idx: ObjIdx) -> Option<Format2TableInfo> {
    let table_links = graph.vertex(table_idx)?.real_links();

    let coverage_idx = table_links.get(&PairPosFormat2::COVERAGE_OFFSET_POS)?.obj_idx();

    let class_def1_idx = table_links.get(&PairPosFormat2::CLASS_DEF1_OFFSET_POS)?.obj_idx();

    let class_def2_idx = table_links.get(&PairPosFormat2::CLASS_DEF2_OFFSET_POS)?.obj_idx();

    let format2_table = PairPosFormat2::from_graph(graph, table_idx).ok()?;
    let value_format1 = format2_table.value_format1();
    let value_format2 = format2_table.value_format2();
    let class1_count = format2_table.class1_count();
    let class2_count = format2_table.class2_count();

    let v1_len = (value_format1 & 0xFF).count_ones();
    let v2_len = (value_format2 & 0xFF).count_ones();
    let class1_record_size = (v1_len + v2_len) as usize * class2_count as usize * 2;
    let total_value_len = v1_len + v2_len;
    let format1_device_indices = get_device_table_indices(value_format1);
    let format2_device_indices = get_device_table_indices(value_format2);

    let glyph_classes = get_glyph_classes(graph, coverage_idx, class_def1_idx).ok()?;

    Some(Format2TableInfo {
        table_idx,
        coverage_idx,
        value_format1,
        value_format2,
        class_def1_idx,
        class_def2_idx,
        class1_count,
        class2_count,
        glyph_classes,
        total_value_len,
        v1_len,
        class1_record_size,
        format1_device_indices,
        format2_device_indices,
    })
}

fn compute_format2_split_points(
    graph: &mut Graph,
    table_info: &Format2TableInfo,
) -> Result<Vec<u16>, RepackError> {
    let class_def2_table_v =
        graph.vertex(table_info.class_def2_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
    let table_real_links = graph
        .vertex(table_info.table_idx)
        .ok_or(RepackError::GraphErrorInvalidObjIndex)?
        .real_links();

    let class_def_2_size = class_def2_table_v.table_size();

    let mut estimator = ClassDefSizeEstimator::new(&table_info.glyph_classes);
    let (format1_device_indices, format2_device_indices) =
        (&table_info.format1_device_indices, &table_info.format2_device_indices);

    let has_device_tables =
        !format1_device_indices.is_empty() || !format2_device_indices.is_empty();

    let mut accumulated = PairPosFormat2::MIN_SIZE;
    let mut split_points = Vec::new();
    let mut visited = IntSet::empty();

    let class2_count = table_info.class2_count as u32;
    for i in 0..table_info.class1_count {
        let mut accumulated_delta = table_info.class1_record_size;
        let class_def_1_size = estimator.add_class_def_size(i);
        let coverage_size = estimator.coverage_size();

        if has_device_tables {
            for j in 0..class2_count {
                let value1_index = table_info.total_value_len * (class2_count * i as u32 + j);
                let value2_index = value1_index + table_info.v1_len;

                accumulated_delta += size_of_value_record_children(
                    graph,
                    table_real_links,
                    format1_device_indices,
                    value1_index,
                    &mut visited,
                )?;
                accumulated_delta += size_of_value_record_children(
                    graph,
                    table_real_links,
                    format2_device_indices,
                    value2_index,
                    &mut visited,
                )?;
            }
        }

        accumulated += accumulated_delta;
        // The largest object will pack last and can exceed the size limit.
        // ref: >https://github.com/harfbuzz/harfbuzz/blob/5b6645dabbc5374ad031f8730c1f1f71e096a6a6/src/graph/pairpos-graph.hh#L274>
        let total = accumulated + coverage_size + class_def_1_size + class_def_2_size
            - coverage_size.max(class_def_1_size.max(class_def_2_size));

        if total >= (1 << 16) {
            split_points.push(i);
            accumulated = PairPosFormat2::MIN_SIZE + accumulated_delta;
            estimator.reset();
            estimator.add_class_def_size(i);
            visited.clear();
        }
    }

    Ok(split_points)
}

fn clone_range_format2(
    graph: &mut Graph,
    start: u16,
    end: u16,
    table_info: &Format2TableInfo,
) -> Result<ObjIdx, RepackError> {
    let num_new_class1 = end - start;
    let new_table_size =
        PairPosFormat2::MIN_SIZE + (num_new_class1 as usize) * table_info.class1_record_size;
    let new_table_idx = graph.new_vertex(new_table_size)?;

    let mut new_table = PairPosFormat2::from_graph(graph, new_table_idx)?;
    new_table.set_format(2);
    new_table.set_value_format1(table_info.value_format1);
    new_table.set_value_format2(table_info.value_format2);
    new_table.set_class1_count(num_new_class1);
    new_table.set_class2_count(table_info.class2_count);

    clone_class_records(graph, start, end, table_info, new_table_idx)?;
    // new coverage glyphs and classes
    let cap = table_info.glyph_classes.len();
    let mut new_cov_glyphs = Vec::with_capacity(cap);
    let mut gid_and_new_classes = Vec::with_capacity(cap);

    for &(g, class) in table_info.glyph_classes.iter().filter(|(_, c)| *c >= start && *c < end) {
        new_cov_glyphs.push(g);
        gid_and_new_classes.push((g.to_u32() as u16, class - start));
    }

    add_new_coverage(
        graph,
        &new_cov_glyphs,
        new_table_idx,
        LinkWidth::Two,
        PairPosFormat2::COVERAGE_OFFSET_POS,
    )?;

    add_new_class_def(
        graph,
        new_table_idx,
        &gid_and_new_classes,
        PairPosFormat2::CLASS_DEF1_OFFSET_POS,
    )?;

    // Link ClassDef2
    graph.add_parent_child_link(
        new_table_idx,
        table_info.class_def2_idx,
        LinkWidth::Two,
        PairPosFormat2::CLASS_DEF2_OFFSET_POS,
        false,
    )?;

    Ok(new_table_idx)
}

fn shrink_format2(
    graph: &mut Graph,
    table_info: &Format2TableInfo,
    shrink_point: u16,
) -> Result<(), RepackError> {
    if shrink_point >= table_info.class1_count {
        return Ok(());
    }

    PairPosFormat2::from_graph(graph, table_info.table_idx)?.set_class1_count(shrink_point);
    let table_v = graph.mut_vertex(table_info.table_idx).unwrap();
    table_v.tail -=
        (table_info.class1_count - shrink_point) as usize * table_info.class1_record_size;

    let cap = table_info.glyph_classes.len();
    let mut new_cov_glyphs = Vec::with_capacity(cap);
    let mut gid_and_new_classes = Vec::with_capacity(cap);

    for &(g, class) in table_info.glyph_classes.iter().filter(|(_, c)| *c < shrink_point) {
        new_cov_glyphs.push(g);
        gid_and_new_classes.push((g.to_u32() as u16, class));
    }

    make_coverage(graph, table_info.coverage_idx, &new_cov_glyphs, 0..new_cov_glyphs.len())?;

    make_class_def(graph, table_info.class_def1_idx, &gid_and_new_classes)
}

struct ClassDefSizeEstimator {
    class_to_glyphs: Vec<IntSet<u32>>,
    num_ranges_per_class: Vec<usize>,
    included_classes: IntSet<u16>,
    included_glyphs: IntSet<u32>,
    format1_size: usize,
    format2_size: usize,
}

impl ClassDefSizeEstimator {
    const COVERAGE_MIN_SIZE: usize = 4;
    const BYTES_PER_GLYPH: usize = 2;
    const BYTES_PER_RANGE: usize = 6;
    const CLASSDEF_FORMAT1_MIN_SIZE: usize = 6;
    const CLASSDEF_FORMAT2_MIN_SIZE: usize = 4;

    fn new(gid_and_class: &[(GlyphId, u16)]) -> Self {
        if gid_and_class.is_empty() {
            return Self {
                class_to_glyphs: Vec::new(),
                num_ranges_per_class: Vec::new(),
                included_glyphs: IntSet::empty(),
                included_classes: IntSet::empty(),
                format1_size: Self::CLASSDEF_FORMAT1_MIN_SIZE,
                format2_size: Self::CLASSDEF_FORMAT2_MIN_SIZE,
            };
        }
        let num_classes = gid_and_class.iter().map(|&(_, c)| c).max().unwrap_or(0) as usize + 1;
        let mut class_to_glyphs = vec![IntSet::empty(); num_classes];
        let mut num_ranges_per_class = vec![0_usize; num_classes];

        for (gid, class) in gid_and_class {
            class_to_glyphs[*class as usize].insert(gid.to_u32());
        }

        for (glyphs, num_ranges) in
            class_to_glyphs.iter().zip(num_ranges_per_class.iter_mut()).skip(1)
        {
            *num_ranges = glyphs.iter_ranges().count();
        }
        Self {
            class_to_glyphs,
            num_ranges_per_class,
            included_glyphs: IntSet::empty(),
            included_classes: IntSet::empty(),
            format1_size: Self::CLASSDEF_FORMAT1_MIN_SIZE,
            format2_size: Self::CLASSDEF_FORMAT2_MIN_SIZE,
        }
    }

    fn add_class_def_size(&mut self, class: u16) -> usize {
        let cur_size = self.format1_size.min(self.format2_size);
        if !self.included_classes.insert(class) {
            return cur_size;
        }

        let Some(glyphs) = self.class_to_glyphs.get(class as usize) else {
            return cur_size;
        };

        if glyphs.is_empty() {
            return cur_size;
        }
        self.included_glyphs.union(glyphs);

        let min_glyph = self.included_glyphs.first().unwrap();
        let max_glyph = self.included_glyphs.last().unwrap();
        self.format1_size = Self::CLASSDEF_FORMAT1_MIN_SIZE
            + Self::BYTES_PER_GLYPH * (max_glyph - min_glyph + 1) as usize;

        let num_ranges = self.num_ranges_per_class.get(class as usize).unwrap_or(&0);
        self.format2_size += Self::BYTES_PER_RANGE * *num_ranges;

        self.format1_size.min(self.format2_size)
    }

    fn coverage_size(&self) -> usize {
        let format_1_size =
            Self::COVERAGE_MIN_SIZE + Self::BYTES_PER_GLYPH * self.included_glyphs.len() as usize;

        let format_2_size = Self::COVERAGE_MIN_SIZE
            + Self::BYTES_PER_RANGE * self.included_glyphs.iter_ranges().count();

        format_1_size.min(format_2_size)
    }

    fn reset(&mut self) {
        self.format1_size = Self::CLASSDEF_FORMAT1_MIN_SIZE;
        self.format2_size = Self::CLASSDEF_FORMAT2_MIN_SIZE;
        self.included_classes.clear();
        self.included_glyphs.clear();
    }
}

fn get_device_table_indices(val: u16) -> Vec<u8> {
    let value_format = ValueFormat::from_bits_truncate(val);
    let mut indices = Vec::new();
    let mut i = 0;
    if value_format.contains(ValueFormat::X_PLACEMENT) {
        i += 1;
    }

    if value_format.contains(ValueFormat::Y_PLACEMENT) {
        i += 1;
    }

    if value_format.contains(ValueFormat::X_ADVANCE) {
        i += 1;
    }

    if value_format.contains(ValueFormat::Y_ADVANCE) {
        i += 1;
    }

    if value_format.contains(ValueFormat::X_PLACEMENT_DEVICE) {
        indices.push(i);
        i += 1;
    }

    if value_format.contains(ValueFormat::Y_PLACEMENT_DEVICE) {
        indices.push(i);
        i += 1;
    }

    if value_format.contains(ValueFormat::X_ADVANCE_DEVICE) {
        indices.push(i);
        i += 1;
    }

    if value_format.contains(ValueFormat::Y_ADVANCE_DEVICE) {
        indices.push(i);
    }

    indices
}

fn size_of_value_record_children(
    graph: &Graph,
    links: &BTreeMap<u32, Link>,
    device_table_indices: &[u8],
    value_record_index: u32,
    visited: &mut IntSet<u32>,
) -> Result<usize, RepackError> {
    let mut size = 0;
    let record_start_pos = PairPosFormat2::MIN_SIZE as u32 + value_record_index * 2;
    for &i in device_table_indices {
        let pos = record_start_pos + i as u32 * 2;
        if let Some(&link) = links.get(&pos) {
            size += graph.find_subgraph_size(link.obj_idx(), visited, u16::MAX)?;
        }
    }
    Ok(size)
}

fn clone_class_records(
    graph: &mut Graph,
    start: u16,
    end: u16,
    table_info: &Format2TableInfo,
    new_table_idx: ObjIdx,
) -> Result<(), RepackError> {
    let (
        table_idx,
        class2_count,
        total_value_len,
        v1_len,
        format1_device_indices,
        format2_device_indices,
    ) = (
        table_info.table_idx,
        table_info.class2_count,
        table_info.total_value_len,
        table_info.v1_len,
        &table_info.format1_device_indices,
        &table_info.format2_device_indices,
    );
    // Copy records
    let copy_size = (end - start) as usize * table_info.class1_record_size;
    let start_pos = graph.vertex(table_info.table_idx).ok_or(RepackError::ErrorSplitSubtable)?.head
        + PairPosFormat2::MIN_SIZE
        + start as usize * table_info.class1_record_size;

    let new_pos = graph.vertex(new_table_idx).ok_or(RepackError::ErrorSplitSubtable)?.head
        + PairPosFormat2::MIN_SIZE;

    graph.data.copy_within(start_pos..start_pos + copy_size, new_pos);

    if format1_device_indices.is_empty() && format2_device_indices.is_empty() {
        return Ok(());
    }
    // Handle device tables
    for i in start..end {
        for j in 0..class2_count {
            let old_value_record_index = total_value_len * (class2_count * i + j) as u32;
            let new_value_record_index =
                old_value_record_index - total_value_len * (class2_count * start) as u32;
            transfer_device_tables(
                graph,
                table_idx,
                new_table_idx,
                format1_device_indices,
                old_value_record_index,
                new_value_record_index,
            )?;
            transfer_device_tables(
                graph,
                table_idx,
                new_table_idx,
                format2_device_indices,
                old_value_record_index + v1_len,
                new_value_record_index + v1_len,
            )?;
        }
    }
    Ok(())
}

fn transfer_device_tables(
    graph: &mut Graph,
    old_table_idx: ObjIdx,
    new_table_idx: ObjIdx,
    device_table_indices: &[u8],
    old_value_record_index: u32,
    new_value_record_index: u32,
) -> Result<(), RepackError> {
    for &i in device_table_indices {
        let old_pos = PairPosFormat2::MIN_SIZE as u32 + (old_value_record_index + i as u32) * 2;
        let new_pos = PairPosFormat2::MIN_SIZE as u32 + (new_value_record_index + i as u32) * 2;
        graph.move_child(old_table_idx, old_pos, new_table_idx, new_pos, Offset16::RAW_BYTE_LEN)?;
    }
    Ok(())
}

fn get_glyph_classes(
    graph: &Graph,
    coverage_idx: ObjIdx,
    class_def_idx: ObjIdx,
) -> Result<Vec<(GlyphId, u16)>, RepackError> {
    let coverage_data =
        graph.vertex_data(coverage_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

    let coverage_table = CoverageTable::read(FontData::new(coverage_data))
        .map_err(|_| RepackError::ErrorReadTable)?;

    let class_def_data =
        graph.vertex_data(class_def_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
    let class_def = ClassDef::read(write_fonts::read::FontData::new(class_def_data))
        .map_err(|_| RepackError::ErrorReadTable)?;

    Ok(coverage_table.iter().map(|g| (GlyphId::from(g), class_def.get(g))).collect())
}

struct PairPosFormat1<'a>(DataBytes<'a>);
impl<'a> PairPosFormat1<'a> {
    const MIN_SIZE: usize = 10;
    const FORMAT_POS: usize = 0;
    const COVERAGE_OFFSET_POS: u32 = 2;
    const VALUE_FORMAT1_POS: usize = 4;
    const VALUE_FORMAT2_POS: usize = 6;
    const PAIR_SET_COUNT_POS: usize = 8;
    const PAIR_SET_OFFSETS_START: u32 = 10;

    fn from_graph(graph: &'a mut Graph, obj_idx: ObjIdx) -> Result<Self, RepackError> {
        let data_bytes = DataBytes::from_graph(graph, obj_idx)?;
        let pair_pos = Self(data_bytes);
        if !pair_pos.sanitize() {
            return Err(RepackError::ErrorReadTable);
        }
        Ok(pair_pos)
    }

    fn sanitize(&self) -> bool {
        self.0.len() >= Self::MIN_SIZE
    }

    fn set_format(&mut self, format: u16) {
        self.0.write_at(format, Self::FORMAT_POS);
    }

    fn value_format1(&self) -> u16 {
        self.0.read_at::<u16>(Self::VALUE_FORMAT1_POS)
    }

    fn set_value_format1(&mut self, format: u16) {
        self.0.write_at(format, Self::VALUE_FORMAT1_POS);
    }

    fn value_format2(&self) -> u16 {
        self.0.read_at::<u16>(Self::VALUE_FORMAT2_POS)
    }

    fn set_value_format2(&mut self, format: u16) {
        self.0.write_at(format, Self::VALUE_FORMAT2_POS);
    }

    fn num_pair_sets(&self) -> u16 {
        self.0.read_at::<u16>(Self::PAIR_SET_COUNT_POS)
    }

    fn set_pair_set_count(&mut self, count: u16) {
        self.0.write_at(count, Self::PAIR_SET_COUNT_POS);
    }
}

struct PairPosFormat2<'a>(DataBytes<'a>);

impl<'a> PairPosFormat2<'a> {
    const MIN_SIZE: usize = 16;
    const FORMAT_POS: usize = 0;
    const COVERAGE_OFFSET_POS: u32 = 2;
    const VALUE_FORMAT1_POS: usize = 4;
    const VALUE_FORMAT2_POS: usize = 6;
    const CLASS_DEF1_OFFSET_POS: u32 = 8;
    const CLASS_DEF2_OFFSET_POS: u32 = 10;
    const CLASS1_COUNT_POS: usize = 12;
    const CLASS2_COUNT_POS: usize = 14;

    fn from_graph(graph: &'a mut Graph, obj_idx: ObjIdx) -> Result<Self, RepackError> {
        let data_bytes = DataBytes::from_graph(graph, obj_idx)?;
        let table = Self(data_bytes);
        if !table.sanitize() {
            return Err(RepackError::ErrorReadTable);
        }
        Ok(table)
    }

    fn sanitize(&self) -> bool {
        self.0.len() >= Self::MIN_SIZE
    }

    fn set_format(&mut self, format: u16) {
        self.0.write_at(format, Self::FORMAT_POS);
    }

    fn value_format1(&self) -> u16 {
        self.0.read_at::<u16>(Self::VALUE_FORMAT1_POS)
    }

    fn set_value_format1(&mut self, val: u16) {
        self.0.write_at(val, Self::VALUE_FORMAT1_POS);
    }

    fn value_format2(&self) -> u16 {
        self.0.read_at::<u16>(Self::VALUE_FORMAT2_POS)
    }

    fn set_value_format2(&mut self, val: u16) {
        self.0.write_at(val, Self::VALUE_FORMAT2_POS);
    }

    fn class1_count(&self) -> u16 {
        self.0.read_at::<u16>(Self::CLASS1_COUNT_POS)
    }

    fn set_class1_count(&mut self, count: u16) {
        self.0.write_at(count, Self::CLASS1_COUNT_POS);
    }

    fn class2_count(&self) -> u16 {
        self.0.read_at::<u16>(Self::CLASS2_COUNT_POS)
    }

    fn set_class2_count(&mut self, count: u16) {
        self.0.write_at(count, Self::CLASS2_COUNT_POS);
    }
}

// Make a ClassDef table at the specified classdef vertex
fn make_class_def(
    graph: &mut Graph,
    dest_idx: ObjIdx,
    glyph_classes: &[(u16, u16)],
) -> Result<(), RepackError> {
    let mut s = Serializer::new(glyph_classes.len() * 6 + 4);
    s.start_serialize().map_err(|_| RepackError::ErrorRepackSerialize)?;

    ClassDef::serialize(&mut s, glyph_classes).map_err(|_| RepackError::ErrorRepackSerialize)?;
    s.end_serialize();

    let classdef_data = s.copy_bytes();
    graph.update_vertex_data(dest_idx, &classdef_data)
}

fn add_new_class_def(
    graph: &mut Graph,
    parent_idx: ObjIdx,
    glyph_classes: &[(u16, u16)],
    position: u32,
) -> Result<ObjIdx, RepackError> {
    let new_class_def_idx = graph.new_vertex(0)?;
    make_class_def(graph, new_class_def_idx, glyph_classes)?;

    graph.add_parent_child_link(parent_idx, new_class_def_idx, LinkWidth::Two, position, false)?;
    Ok(new_class_def_idx)
}

#[cfg(test)]
pub(crate) mod test {
    use super::*;

    fn actual_class_def_size(glyph_and_classes: &[(u16, u16)]) -> usize {
        let mut s = Serializer::new(100);
        s.start_serialize().unwrap();
        ClassDef::serialize(&mut s, glyph_and_classes).unwrap();
        s.end_serialize();
        assert!(!s.in_error());
        s.copy_bytes().len()
    }

    fn actual_coverage_size(glyphs: &[GlyphId]) -> usize {
        let mut s = Serializer::new(100);
        s.start_serialize().unwrap();

        CoverageTable::serialize(&mut s, glyphs).unwrap();
        s.end_serialize();
        s.copy_bytes().len()
    }

    fn check_add_class_def_size(glyph_and_classes: &[(GlyphId, u16)], class: u16) {
        let mut estimator = ClassDefSizeEstimator::new(glyph_and_classes);
        let est_class_def_size = estimator.add_class_def_size(class);

        let mut filtered_glyph_classes = Vec::new();
        let mut filtered_glyphs = Vec::new();
        for &(g, class) in glyph_and_classes.iter().filter(|(_, c)| *c == class) {
            filtered_glyph_classes.push((g.to_u32() as u16, class));
            filtered_glyphs.push(g);
        }
        let actual_class_def_size = actual_class_def_size(&filtered_glyph_classes);
        assert_eq!(est_class_def_size, actual_class_def_size);

        let est_cov_size = estimator.coverage_size();
        let actual_cov_size = actual_coverage_size(&filtered_glyphs);
        assert_eq!(est_cov_size, actual_cov_size);
    }

    fn check_add_class_def_sizes(
        estimator: &mut ClassDefSizeEstimator,
        glyph_and_classes: &[(GlyphId, u16)],
        class: u16,
        classes: &IntSet<u16>,
    ) {
        let est_class_def_size = estimator.add_class_def_size(class);

        let mut filtered_glyph_classes = Vec::new();
        let mut filtered_glyphs = Vec::new();
        for &(g, class) in glyph_and_classes.iter().filter(|(_, c)| classes.contains(*c)) {
            filtered_glyph_classes.push((g.to_u32() as u16, class));
            filtered_glyphs.push(g);
        }

        let actual_class_def_size = actual_class_def_size(&filtered_glyph_classes);
        assert_eq!(est_class_def_size, actual_class_def_size);

        let est_cov_size = estimator.coverage_size();
        let actual_cov_size = actual_coverage_size(&filtered_glyphs);
        assert_eq!(est_cov_size, actual_cov_size);
    }

    #[test]
    fn test_class_and_coverage_size_estimates() {
        let empty = Vec::new();
        check_add_class_def_size(&empty, 0);
        check_add_class_def_size(&empty, 1);

        let class_zero = vec![(GlyphId::from(5_u16), 0)];
        check_add_class_def_size(&class_zero, 0);

        let consecutive = [
            (GlyphId::from(4_u16), 0),
            (GlyphId::from(5_u16), 0),
            (GlyphId::from(6_u16), 1),
            (GlyphId::from(7_u16), 1),
            (GlyphId::from(8_u16), 2),
            (GlyphId::from(9_u16), 2),
            (GlyphId::from(10_u16), 2),
            (GlyphId::from(11_u16), 2),
        ];
        check_add_class_def_size(&consecutive, 0);
        check_add_class_def_size(&consecutive, 1);
        check_add_class_def_size(&consecutive, 2);

        let non_consecutive = [
            (GlyphId::from(4_u16), 0),
            (GlyphId::from(6_u16), 0),
            (GlyphId::from(8_u16), 1),
            (GlyphId::from(10_u16), 1),
            (GlyphId::from(9_u16), 2),
            (GlyphId::from(10_u16), 2),
            (GlyphId::from(11_u16), 2),
            (GlyphId::from(13_u16), 2),
        ];
        check_add_class_def_size(&non_consecutive, 0);
        check_add_class_def_size(&non_consecutive, 1);
        check_add_class_def_size(&non_consecutive, 2);

        let multiple_ranges = [
            (GlyphId::from(4_u16), 0),
            (GlyphId::from(5_u16), 0),
            (GlyphId::from(6_u16), 1),
            (GlyphId::from(7_u16), 1),
            (GlyphId::from(9_u16), 1),
            (GlyphId::from(11_u16), 1),
            (GlyphId::from(12_u16), 1),
            (GlyphId::from(13_u16), 1),
        ];
        check_add_class_def_size(&multiple_ranges, 0);
        check_add_class_def_size(&multiple_ranges, 1);
    }

    #[test]
    fn test_running_class_and_coverage_size_estimates() {
        // With consecutive gids: switches formats
        let consecutive_gids = [
            // range 1-4 (f1: 8 bytes), (f2: 6 bytes)
            (GlyphId::from(1_u16), 1),
            (GlyphId::from(2_u16), 1),
            (GlyphId::from(3_u16), 1),
            (GlyphId::from(4_u16), 1),
            // (f1: 2 bytes), (f2: 6 bytes)
            (GlyphId::from(5_u16), 2),
            // (f1: 14 bytes), (f2: 6 bytes)
            (GlyphId::from(6_u16), 3),
            (GlyphId::from(7_u16), 3),
            (GlyphId::from(8_u16), 3),
            (GlyphId::from(9_u16), 3),
            (GlyphId::from(10_u16), 3),
            (GlyphId::from(11_u16), 3),
            (GlyphId::from(12_u16), 3),
        ];

        let mut estimator = ClassDefSizeEstimator::new(&consecutive_gids);
        let mut classes = IntSet::empty();
        classes.insert(1_u16);
        check_add_class_def_sizes(&mut estimator, &consecutive_gids, 1, &classes);

        classes.insert(2);
        check_add_class_def_sizes(&mut estimator, &consecutive_gids, 2, &classes);
        // check that adding the same class again works
        check_add_class_def_sizes(&mut estimator, &consecutive_gids, 2, &classes);

        classes.insert(3);
        check_add_class_def_sizes(&mut estimator, &consecutive_gids, 3, &classes);

        estimator.reset();
        classes.remove(1);
        classes.remove(3);
        check_add_class_def_sizes(&mut estimator, &consecutive_gids, 2, &classes);

        classes.insert(3);
        check_add_class_def_sizes(&mut estimator, &consecutive_gids, 3, &classes);

        // With non-consecutive gids: always uses format 2 ###
        let non_consecutive_gids = [
            // range 1-4 (f1: 8 bytes), (f2: 6 bytes)
            (GlyphId::from(1_u16), 1),
            (GlyphId::from(2_u16), 1),
            (GlyphId::from(3_u16), 1),
            (GlyphId::from(4_u16), 1),
            // (f1: 2 bytes), (f2: 12 bytes)
            (GlyphId::from(6_u16), 2),
            (GlyphId::from(8_u16), 2),
            // (f1: 14 bytes), (f2: 6 bytes)
            (GlyphId::from(9_u16), 3),
            (GlyphId::from(10_u16), 3),
            (GlyphId::from(11_u16), 3),
            (GlyphId::from(12_u16), 3),
            (GlyphId::from(13_u16), 3),
            (GlyphId::from(14_u16), 3),
            (GlyphId::from(15_u16), 3),
        ];

        let mut estimator2 = ClassDefSizeEstimator::new(&non_consecutive_gids);
        classes.clear();
        classes.insert(1_u16);
        check_add_class_def_sizes(&mut estimator2, &non_consecutive_gids, 1, &classes);

        classes.insert(2);
        check_add_class_def_sizes(&mut estimator2, &non_consecutive_gids, 2, &classes);

        classes.insert(3);
        check_add_class_def_sizes(&mut estimator2, &non_consecutive_gids, 3, &classes);

        estimator2.reset();
        classes.remove(1);
        classes.remove(3);
        check_add_class_def_sizes(&mut estimator2, &non_consecutive_gids, 2, &classes);

        classes.insert(3);
        check_add_class_def_sizes(&mut estimator2, &non_consecutive_gids, 3, &classes);
    }

    #[test]
    fn test_running_class_size_estimates_with_locally_consecutive_glyphs() {
        let gids_and_classes =
            [(GlyphId::from(1_u16), 1), (GlyphId::from(6_u16), 2), (GlyphId::from(7_u16), 3)];

        let mut estimator = ClassDefSizeEstimator::new(&gids_and_classes);
        let mut classes = IntSet::empty();
        classes.insert(1_u16);
        check_add_class_def_sizes(&mut estimator, &gids_and_classes, 1, &classes);

        classes.insert(2);
        check_add_class_def_sizes(&mut estimator, &gids_and_classes, 2, &classes);

        classes.insert(3);
        check_add_class_def_sizes(&mut estimator, &gids_and_classes, 3, &classes);

        estimator.reset();
        classes.remove(1);
        classes.remove(3);
        check_add_class_def_sizes(&mut estimator, &gids_and_classes, 2, &classes);

        classes.insert(3);
        check_add_class_def_sizes(&mut estimator, &gids_and_classes, 3, &classes);
    }
}
