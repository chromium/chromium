//! Split MarkBasePos table in a graph
use crate::{
    graph::{
        coverage_graph::{add_new_coverage, coverage_glyphs, make_coverage},
        layout::DataBytes,
        Graph, RepackError,
    },
    serialize::{LinkWidth, ObjIdx},
};
use write_fonts::{
    read::collections::IntSet,
    types::{FixedSize, GlyphId, Offset16},
};

// output only contains new subtable indices
// ref:<https://github.com/harfbuzz/harfbuzz/blob/fa2908bf16d2ccd6623f4d575455fea72a1a722b/src/graph/markbasepos-graph.hh#L214>
pub(crate) fn split_markbase_pos(
    graph: &mut Graph,
    table_idx: ObjIdx,
) -> Result<Vec<ObjIdx>, RepackError> {
    let Some(table_info) = get_table_info(graph, table_idx) else {
        return Ok(Vec::new());
    };

    let split_points = compute_split_points(graph, &table_info)?;
    if split_points.is_empty() {
        return Ok(Vec::new());
    }

    let mark_cov_glyphs = coverage_glyphs(graph, table_info.mark_coverage_idx)?;
    let num_split_points = split_points.len();
    let mut out: Vec<usize> = Vec::with_capacity(num_split_points);
    for i in 0..split_points.len() {
        // [start,end) range
        let start = split_points[i];
        let end = if i < num_split_points - 1 {
            split_points[i + 1]
        } else {
            table_info.mark_class_count
        };

        let new_idx = clone_range(graph, &table_info, start as u16, end as u16, &mark_cov_glyphs)?;
        out.push(new_idx);
    }

    shrink(graph, &table_info, &mark_cov_glyphs, split_points[0])?;
    Ok(out)
}

struct TableInfo {
    table_idx: ObjIdx,
    mark_coverage_idx: ObjIdx,
    base_coverage_idx: ObjIdx,
    mark_array_idx: ObjIdx,
    base_array_idx: ObjIdx,
    mark_class_count: usize,
    base_count: usize,
    class_mark_indices: Vec<IntSet<u16>>,
    mark_classes: Vec<u16>,
}

fn get_table_info(graph: &mut Graph, table_idx: ObjIdx) -> Option<TableInfo> {
    let table_links = graph.vertex(table_idx)?.real_links();

    let mark_coverage_idx =
        table_links.get(&MarkBasePosFormat1::MARK_COVERAGE_OFFSET_POS)?.obj_idx();

    let base_coverage_idx =
        table_links.get(&MarkBasePosFormat1::BASE_COVERAGE_OFFSET_POS)?.obj_idx();

    let mark_array_idx = table_links.get(&MarkBasePosFormat1::MARK_ARRAY_OFFSET_POS)?.obj_idx();

    let base_array_idx = table_links.get(&MarkBasePosFormat1::BASE_ARRAY_OFFSET_POS)?.obj_idx();

    let mark_class_count =
        MarkBasePosFormat1::from_graph(graph, table_idx).ok()?.mark_class_count() as usize;
    let base_count =
        BaseArray::from_graph(graph, base_array_idx, mark_class_count).ok()?.base_count() as usize;

    let (class_mark_indices, mark_classes) =
        get_class_mark_indices_map(graph, mark_class_count, mark_array_idx).ok()?;

    Some(TableInfo {
        table_idx,
        mark_coverage_idx,
        base_coverage_idx,
        mark_array_idx,
        base_array_idx,
        mark_class_count,
        base_count,
        class_mark_indices,
        mark_classes,
    })
}

fn get_class_mark_indices_map(
    graph: &mut Graph,
    mark_class_count: usize,
    mark_array_idx: ObjIdx,
) -> Result<(Vec<IntSet<u16>>, Vec<u16>), RepackError> {
    let mark_array = MarkArray::from_graph(graph, mark_array_idx)?;
    let mark_count = mark_array.mark_count() as usize;
    let mut class_mark_indices = vec![IntSet::empty(); mark_class_count];
    let mut mark_classes = vec![0; mark_count];

    for ((i, mark_class), class) in
        mark_array.iter_mark_index_and_class().zip(mark_classes.iter_mut())
    {
        class_mark_indices
            .get_mut(mark_class as usize)
            .ok_or(RepackError::ErrorReadTable)?
            .insert(i as u16);
        *class = mark_class;
    }
    Ok((class_mark_indices, mark_classes))
}

// ref:<https://github.com/harfbuzz/harfbuzz/blob/aba63bb5f8cb6cfc77ee8cfc2700b3ed9c0838ef/src/graph/markbasepos-graph.hh#L239>
fn compute_split_points(
    graph: &mut Graph,
    table_info: &TableInfo,
) -> Result<Vec<usize>, RepackError> {
    let base_cov_table_size = graph
        .vertex(table_info.base_coverage_idx)
        .ok_or(RepackError::GraphErrorInvalidObjIndex)?
        .table_size();

    let base_size = MarkBasePosFormat1::MIN_SIZE
        + MarkArray::MIN_SIZE
        + BaseArray::MIN_SIZE
        + base_cov_table_size;

    let mark_class_count = table_info.mark_class_count;
    let base_count = table_info.base_count;
    let base_array_idx = table_info.base_array_idx;
    let class_mark_indices = &table_info.class_mark_indices;

    let mut partial_coverage_size = 4;
    let mut accumulated = base_size;
    let mut visited = IntSet::empty();
    let mut out = Vec::new();
    for (class, mark_indices) in class_mark_indices.iter().enumerate() {
        let num_marks_for_class = mark_indices.len() as usize;
        partial_coverage_size += 2 * num_marks_for_class;

        // base record size + mark record size for this class
        let mut delta = base_count * Offset16::RAW_BYTE_LEN + num_marks_for_class * 4;
        for i in 0..base_count {
            let pos = 2 + (i * mark_class_count + class) * Offset16::RAW_BYTE_LEN;
            let Some(base_anchor_idx) = graph.index_for_position(base_array_idx, pos as u32) else {
                continue;
            };
            delta += graph.find_subgraph_size(base_anchor_idx, &mut visited, u16::MAX)?;
        }

        // mark record size for this class
        let mark_array_index = table_info.mark_array_idx;
        for idx in mark_indices.iter() {
            let Some(mark_anchor_idx) =
                graph.index_for_position(mark_array_index, 4 + 4 * idx as u32)
            else {
                continue;
            };
            delta += graph.find_subgraph_size(mark_anchor_idx, &mut visited, u16::MAX)?;
        }

        accumulated += delta;
        if accumulated + partial_coverage_size > u16::MAX as usize {
            out.push(class);
            accumulated = base_size + delta;
            partial_coverage_size = 4 + 2 * num_marks_for_class;
            visited.clear();
        }
    }
    Ok(out)
}

// Create a new MarkBasePos that has all of the data for classes from [start,
// end). ref:<https://github.com/harfbuzz/harfbuzz/blob/aba63bb5f8cb6cfc77ee8cfc2700b3ed9c0838ef/src/graph/markbasepos-graph.hh#L412>
fn clone_range(
    graph: &mut Graph,
    table_info: &TableInfo,
    start: u16,
    end: u16,
    org_mark_cov_glyphs: &[GlyphId],
) -> Result<ObjIdx, RepackError> {
    let new_markbase_pos_idx = graph.new_vertex(MarkBasePosFormat1::MIN_SIZE)?;
    let new_mark_class_count = end - start;

    let mut new_markbase_pos = MarkBasePosFormat1::from_graph(graph, new_markbase_pos_idx)?;
    new_markbase_pos.set_format(1);
    new_markbase_pos.set_mark_class_count(new_mark_class_count);

    // link to base coverage
    graph.add_parent_child_link(
        new_markbase_pos_idx,
        table_info.base_coverage_idx,
        LinkWidth::Two,
        MarkBasePosFormat1::BASE_COVERAGE_OFFSET_POS,
        false,
    )?;

    // add new mark coverage
    let cap = org_mark_cov_glyphs.len();
    let mut new_mark_glyphs = Vec::with_capacity(cap);
    let mut mark_classes = Vec::with_capacity(cap);
    let org_mark_classes = &table_info.mark_classes;
    for ((i, &g), &class) in org_mark_cov_glyphs
        .iter()
        .enumerate()
        .zip(org_mark_classes.iter())
        .filter(|(_, class)| (start..end).contains(class))
    {
        new_mark_glyphs.push(g);
        mark_classes.push((i, class));
    }

    add_new_coverage(
        graph,
        &new_mark_glyphs,
        new_markbase_pos_idx,
        LinkWidth::Two,
        MarkBasePosFormat1::MARK_COVERAGE_OFFSET_POS,
    )?;

    add_new_mark_array(
        graph,
        new_markbase_pos_idx,
        table_info.mark_array_idx,
        &mark_classes,
        start,
    )?;

    add_base_array(graph, new_markbase_pos_idx, table_info, start as usize, end as usize)?;
    Ok(new_markbase_pos_idx)
}

fn add_new_mark_array(
    graph: &mut Graph,
    parent_idx: ObjIdx,
    org_mark_array_idx: ObjIdx,
    org_mark_idx_classes: &[(usize, u16)],
    start_class: u16,
) -> Result<(), RepackError> {
    let new_mark_count = org_mark_idx_classes.len();
    let new_mark_array_size = MarkArray::MIN_SIZE + 4 * new_mark_count;
    let new_mark_array_idx = graph.new_vertex(new_mark_array_size)?;

    let mut new_mark_array = MarkArray::from_graph(graph, new_mark_array_idx)?;
    new_mark_array.set_mark_count(new_mark_count as u16);

    for (i, &(_, org_class)) in org_mark_idx_classes.iter().enumerate() {
        new_mark_array.set_mark_class(i, org_class - start_class);
    }

    let start_pos = MarkArray::MIN_SIZE as u32 + 2;
    for (new_idx, &(old_mark_idx, _)) in org_mark_idx_classes.iter().enumerate() {
        let old_pos = start_pos + old_mark_idx as u32 * 4;
        let new_pos = start_pos + new_idx as u32 * 4;
        let _ = graph.move_child(
            org_mark_array_idx,
            old_pos,
            new_mark_array_idx,
            new_pos,
            Offset16::RAW_BYTE_LEN,
        )?;
    }

    graph.add_parent_child_link(
        parent_idx,
        new_mark_array_idx,
        LinkWidth::Two,
        MarkBasePosFormat1::MARK_ARRAY_OFFSET_POS,
        false,
    )
}

fn add_base_array(
    graph: &mut Graph,
    parent_idx: ObjIdx,
    table_info: &TableInfo,
    start_class: usize,
    end_class: usize,
) -> Result<(), RepackError> {
    let new_class_count = end_class - start_class;
    let base_count = table_info.base_count;

    let new_base_array_size = BaseArray::MIN_SIZE + 2 * base_count * new_class_count;
    let new_base_array_idx = graph.new_vertex(new_base_array_size)?;

    let mut new_base_array = BaseArray::from_graph(graph, new_base_array_idx, new_class_count)?;
    new_base_array.set_base_count(base_count as u16);

    let start_pos = BaseArray::MIN_SIZE as u32;
    let org_base_array_idx = table_info.base_array_idx;
    let org_class_count = table_info.mark_class_count;
    for base_idx in 0..base_count {
        let old_pos_start = start_pos + 2 * (base_idx * org_class_count + start_class) as u32;
        let new_pos_start = start_pos + 2 * (base_idx * new_class_count) as u32;
        graph.move_children(
            org_base_array_idx,
            old_pos_start,
            new_base_array_idx,
            new_pos_start,
            new_class_count as u32,
            Offset16::RAW_BYTE_LEN,
        )?;
    }

    graph.add_parent_child_link(
        parent_idx,
        new_base_array_idx,
        LinkWidth::Two,
        MarkBasePosFormat1::BASE_ARRAY_OFFSET_POS,
        false,
    )
}

fn shrink(
    graph: &mut Graph,
    table_info: &TableInfo,
    org_mark_cov_glyphs: &[GlyphId],
    shrink_point: usize,
) -> Result<(), RepackError> {
    let mut mark_base_pos = MarkBasePosFormat1::from_graph(graph, table_info.table_idx)?;
    mark_base_pos.set_mark_class_count(shrink_point as u16);
    // shrink mark coverage
    let cap = org_mark_cov_glyphs.len();
    let mut retained_mark_glyphs = Vec::with_capacity(cap);
    let mut retained_mark_classes = Vec::with_capacity(cap);
    let org_mark_classes = &table_info.mark_classes;
    for ((i, &g), &class) in org_mark_cov_glyphs
        .iter()
        .enumerate()
        .zip(org_mark_classes.iter())
        .filter(|(_, &class)| (class as usize) < shrink_point)
    {
        retained_mark_glyphs.push(g);
        retained_mark_classes.push((i, class));
    }

    make_coverage(
        graph,
        table_info.mark_coverage_idx,
        &retained_mark_glyphs,
        0..retained_mark_glyphs.len(),
    )?;

    shrink_mark_array(graph, table_info.mark_array_idx, &retained_mark_classes)?;

    shrink_base_array(graph, table_info, shrink_point)
}

fn shrink_mark_array(
    graph: &mut Graph,
    mark_array_idx: ObjIdx,
    retained_mark_classes: &[(usize, u16)],
) -> Result<(), RepackError> {
    let mut mark_array = MarkArray::from_graph(graph, mark_array_idx)?;
    let org_mark_count = mark_array.mark_count();
    for (i, class) in retained_mark_classes.iter().map(|(_, c)| *c).enumerate() {
        mark_array.set_mark_class(i, class);
    }

    let new_mark_count = retained_mark_classes.len() as u16;
    mark_array.set_mark_count(new_mark_count);

    let mark_array_v =
        graph.mut_vertex(mark_array_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

    mark_array_v.tail -= 4 * (org_mark_count - new_mark_count) as usize;

    let links = &mut mark_array_v.real_links;
    let mut new_links = Vec::with_capacity(links.len());
    for (new_idx, old_idx) in
        retained_mark_classes.iter().map(|(old_idx, _)| *old_idx as u32).enumerate()
    {
        let old_pos = old_idx * 4 + 4;
        let Some((_, mut l)) = links.remove_entry(&old_pos) else {
            continue;
        };

        let new_pos = new_idx as u32 * 4 + 4;
        l.update_position(new_pos);
        new_links.push((new_pos, l));
    }

    // sanity check
    if !links.is_empty() {
        return Err(RepackError::ErrorSplitSubtable);
    }
    links.extend(new_links);
    Ok(())
}

fn shrink_base_array(
    graph: &mut Graph,
    table_info: &TableInfo,
    shrink_point: usize,
) -> Result<(), RepackError> {
    let base_array_v = graph
        .mut_vertex(table_info.base_array_idx)
        .ok_or(RepackError::GraphErrorInvalidObjIndex)?;

    let mark_class_count = table_info.mark_class_count;
    let base_count = table_info.base_count;
    base_array_v.tail -= (mark_class_count - shrink_point) * Offset16::RAW_BYTE_LEN * base_count;

    let links = &mut base_array_v.real_links;
    let mut new_links = Vec::with_capacity(links.len());
    for i in 0..base_count as u32 {
        for class in 0..shrink_point as u32 {
            let old_pos = 2 + (i * mark_class_count as u32 + class) * 2;
            let Some((_, mut l)) = links.remove_entry(&old_pos) else {
                continue;
            };

            let new_pos = 2 + (i * shrink_point as u32 + class) * 2;
            l.update_position(new_pos);
            new_links.push((new_pos, l));
        }
    }

    // sanity check
    if !links.is_empty() {
        return Err(RepackError::ErrorSplitSubtable);
    }
    links.extend(new_links);
    Ok(())
}

struct MarkBasePosFormat1<'a>(DataBytes<'a>);

impl<'a> MarkBasePosFormat1<'a> {
    const MIN_SIZE: usize = 12;
    const FORMAT_BYTE_POS: u32 = 0;
    const MARK_COVERAGE_OFFSET_POS: u32 = 2;
    const BASE_COVERAGE_OFFSET_POS: u32 = 4;
    const MARK_CLASS_COUNT_POS: u32 = 6;
    const MARK_ARRAY_OFFSET_POS: u32 = 8;
    const BASE_ARRAY_OFFSET_POS: u32 = 10;

    fn from_graph(graph: &'a mut Graph, obj_idx: ObjIdx) -> Result<Self, RepackError> {
        let data_bytes = DataBytes::from_graph(graph, obj_idx)?;
        let markbase_pos = Self(data_bytes);

        if !markbase_pos.sanitize() {
            return Err(RepackError::ErrorReadTable);
        }

        Ok(markbase_pos)
    }

    fn sanitize(&self) -> bool {
        self.0.len() >= Self::MIN_SIZE
    }

    fn mark_class_count(&self) -> u16 {
        self.0.read_at::<u16>(Self::MARK_CLASS_COUNT_POS as usize)
    }

    fn set_format(&mut self, format: u16) {
        self.0.write_at(format, Self::FORMAT_BYTE_POS as usize);
    }

    fn set_mark_class_count(&mut self, mark_class_count: u16) {
        self.0.write_at(mark_class_count, Self::MARK_CLASS_COUNT_POS as usize);
    }
}

struct MarkArray<'a>(DataBytes<'a>);
impl<'a> MarkArray<'a> {
    const MIN_SIZE: usize = 2;
    const MARK_COUNT_POS: usize = 0;

    fn from_graph(graph: &'a mut Graph, obj_idx: ObjIdx) -> Result<Self, RepackError> {
        let data_bytes = DataBytes::from_graph(graph, obj_idx)?;
        let mark_array = Self(data_bytes);

        if !mark_array.sanitize() {
            return Err(RepackError::ErrorReadTable);
        }

        Ok(mark_array)
    }

    fn sanitize(&self) -> bool {
        if self.0.len() < Self::MIN_SIZE {
            return false;
        }

        let mark_count = self.mark_count();
        self.0.len() >= 2 + 4 * mark_count as usize
    }

    fn mark_count(&self) -> u16 {
        self.0.read_at::<u16>(Self::MARK_COUNT_POS)
    }

    fn set_mark_count(&mut self, mark_count: u16) {
        self.0.write_at(mark_count, Self::MARK_COUNT_POS);
    }

    // user is responsible for ensuring no out-of-bound writing
    fn set_mark_class(&mut self, mark_idx: usize, mark_class: u16) {
        let pos = Self::MIN_SIZE + mark_idx * 4;
        self.0.write_at(mark_class, pos);
    }

    fn iter_mark_index_and_class(&self) -> impl Iterator<Item = (usize, u16)> + '_ {
        let mark_count = self.mark_count() as usize;
        let mut iter = 0..mark_count;
        std::iter::from_fn(move || {
            iter.next().map(|i| (i, self.0.read_at::<u16>(Self::MIN_SIZE + i * 4)))
        })
    }
}

struct BaseArray<'a>(DataBytes<'a>);
impl<'a> BaseArray<'a> {
    const MIN_SIZE: usize = 2;
    const BASE_COUNT_POS: usize = 0;

    fn from_graph(
        graph: &'a mut Graph,
        obj_idx: ObjIdx,
        class_count: usize,
    ) -> Result<Self, RepackError> {
        let data_bytes = DataBytes::from_graph(graph, obj_idx)?;
        let base_array = Self(data_bytes);

        if !base_array.sanitize(class_count) {
            return Err(RepackError::ErrorReadTable);
        }
        Ok(base_array)
    }

    fn sanitize(&self, class_count: usize) -> bool {
        if self.0.len() < Self::MIN_SIZE {
            return false;
        }

        let base_count = self.base_count() as usize;
        self.0.len() >= Self::MIN_SIZE + base_count * class_count * Offset16::RAW_BYTE_LEN
    }

    fn base_count(&self) -> u16 {
        self.0.read_at::<u16>(Self::BASE_COUNT_POS)
    }

    fn set_base_count(&mut self, base_count: u16) {
        self.0.write_at(base_count, Self::BASE_COUNT_POS);
    }
}
