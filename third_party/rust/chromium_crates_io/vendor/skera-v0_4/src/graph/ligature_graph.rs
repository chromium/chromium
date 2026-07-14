//! Split ligature substitution table in a graph

use crate::{
    graph::{
        coverage_graph::{coverage_glyphs, make_coverage},
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
// ref:<https://github.com/harfbuzz/harfbuzz/blob/e1f2565db09823794e3d8ed404c47dae0f0cd3c9/src/graph/ligature-graph.hh#L69>
pub(crate) fn split_ligature_subst(
    graph: &mut Graph,
    table_idx: ObjIdx,
) -> Result<Vec<ObjIdx>, RepackError> {
    let Some(coverage_idx) =
        graph.index_for_position(table_idx, LigatureSubstFormat1::COVERAGE_OFFSET_POS as u32)
    else {
        return Ok(Vec::new());
    };

    let lig_set_count = LigatureSubstFormat1::from_graph(graph, table_idx)?.lig_set_count();
    let split_points = compute_split_points(graph, table_idx, lig_set_count)?;
    if split_points.is_empty() {
        return Ok(Vec::new());
    }

    duplicate_shared_liga_sets(graph, table_idx)?;
    let cov_glyphs = coverage_glyphs(graph, coverage_idx)?;
    let mut out: Vec<usize> = Vec::with_capacity(split_points.len() + 1);
    for i in 0..split_points.len() {
        // [start,end) range
        let start = split_points[i];
        let end = if i < split_points.len() - 1 {
            split_points[i + 1]
        } else {
            (lig_set_count as u32) << 16
        };

        let can_move_first_lig_set = i != 0;
        let new_idx = clone_range(
            graph,
            table_idx,
            coverage_idx,
            &cov_glyphs,
            lig_set_count as usize,
            start,
            end,
            can_move_first_lig_set,
        )?;
        out.push(new_idx);
    }

    shrink(graph, table_idx, coverage_idx, &cov_glyphs, split_points[0])?;
    Ok(out)
}

fn duplicate_shared_liga_sets(graph: &mut Graph, table_idx: ObjIdx) -> Result<(), RepackError> {
    let table_v = graph.vertex(table_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
    let mut visited = IntSet::empty();
    let mut duplicates = Vec::new();
    for (pos, l) in table_v.real_links() {
        let obj_idx = l.obj_idx();
        if !visited.insert(obj_idx as u32) {
            duplicates.push((*pos, obj_idx));
        }
    }

    for (pos, obj_idx) in duplicates {
        graph.duplicate_child_at_position(table_idx, obj_idx, pos)?;
    }
    Ok(())
}

// ref:<https://github.com/harfbuzz/harfbuzz/blob/e1f2565db09823794e3d8ed404c47dae0f0cd3c9/src/graph/ligature-graph.hh#L124>
fn compute_split_points(
    graph: &mut Graph,
    this_index: ObjIdx,
    lig_set_count: u16,
) -> Result<Vec<u32>, RepackError> {
    let mut accumulated = LigatureSubstFormat1::MIN_SIZE;
    let mut out = Vec::new();
    for i in 0..lig_set_count as u32 {
        // offset to ligature set + LigatureSet table min_size
        accumulated += Offset16::RAW_BYTE_LEN + LigatureSet::MIN_SIZE;

        let pos = LigatureSubstFormat1::MIN_SIZE + i as usize * Offset16::RAW_BYTE_LEN;
        let Some(ligset_idx) = graph.index_for_position(this_index, pos as u32) else {
            return Ok(Vec::new());
        };

        let lig_set = LigatureSet::from_graph(graph, ligset_idx)?;
        let lig_count = lig_set.lig_count();
        for j in 0..lig_count as u32 {
            let Some(lig_idx) =
                graph.index_for_position(ligset_idx, 2 + j * Offset16::RAW_BYTE_LEN as u32)
            else {
                continue;
            };

            let lig_v = graph.vertex(lig_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

            let lig_size = lig_v.table_size();
            // offset to ligature + ligature table size
            accumulated += Offset16::RAW_BYTE_LEN + lig_size;
            if accumulated > u16::MAX as usize {
                out.push((i << 16) + j);

                // We're going to split such that the current ligature will be in the new sub
                // table. That means we'll have one ligature subst (base_base),
                // one ligature set, and one liga table
                accumulated = LigatureSubstFormat1::MIN_SIZE
                    + Offset16::RAW_BYTE_LEN * 2
                    + LigatureSet::MIN_SIZE
                    + lig_size;
            }
        }
    }
    Ok(out)
}

// ref:<https://github.com/harfbuzz/harfbuzz/blob/e1f2565db09823794e3d8ed404c47dae0f0cd3c9/src/graph/ligature-graph.hh#L269>
#[allow(clippy::too_many_arguments)]
fn clone_range(
    graph: &mut Graph,
    this_index: ObjIdx,
    coverage_idx: ObjIdx,
    cov_glyphs: &[GlyphId],
    lig_set_count: usize,
    start: u32,
    end: u32,
    can_move_first_liga_set: bool,
) -> Result<ObjIdx, RepackError> {
    // Create an oversized new liga subst, we'll adjust the size down later. We
    // don't know the final size until we process it but we also need it to
    // exist while we're processing so that nodes can be moved to it as needed.
    let prime_size = LigatureSubstFormat1::MIN_SIZE + lig_set_count * Offset16::RAW_BYTE_LEN;
    let new_lig_subst_idx = graph.new_vertex(prime_size)?;

    // Create a place holder coverage prime id since we need to add virtual links to
    // it while generating liga and liga sets. Afterwards it will be updated to
    // have the correct coverage.
    let new_coverage_idx = graph.new_vertex(0)?;
    graph.add_parent_child_link(new_lig_subst_idx, new_coverage_idx, LinkWidth::Two, 2, false)?;

    let start_lig_set_idx = start >> 16;
    let start_lig_idx = start & 0xFFFF;

    let end_lig_set_idx = end >> 16;
    let end_lig_idx = end & 0xFFFF;

    let mut new_lig_set_count = 0;
    for cur_lig_set_idx in start_lig_set_idx..=end_lig_set_idx {
        let ligset_pos = LigatureSubstFormat1::MIN_SIZE as u32 + cur_lig_set_idx * 2;
        let Some(ligset_graph_index) = graph.index_for_position(this_index, ligset_pos) else {
            continue;
        };

        let new_ligset_idx = if cur_lig_set_idx == start_lig_set_idx {
            if end_lig_set_idx == start_lig_set_idx
                || (start_lig_idx != 0 && !can_move_first_liga_set)
            {
                // This liga set partially overlaps [start, end). We'll need to create
                // a new liga set sub table and move the intersecting ligas to it.
                let num_moved_liga = if end_lig_set_idx == start_lig_set_idx {
                    end_lig_idx - start_lig_idx
                } else {
                    let num_remaining_ligas = graph
                        .vertex(ligset_graph_index)
                        .ok_or(RepackError::GraphErrorInvalidObjIndex)?
                        .real_links
                        .len() as u32;
                    num_remaining_ligas - start_lig_idx
                };

                let new_ligset_table_idx = create_new_ligature_set(graph, num_moved_liga as u16)?;
                graph.move_children(
                    ligset_graph_index,
                    LigatureSet::MIN_SIZE as u32 + start_lig_idx * Offset16::RAW_BYTE_LEN as u32,
                    new_ligset_table_idx,
                    LigatureSet::MIN_SIZE as u32,
                    num_moved_liga,
                    Offset16::RAW_BYTE_LEN,
                )?;

                // link new ligset
                graph.add_parent_child_link(
                    new_lig_subst_idx,
                    new_ligset_table_idx,
                    LinkWidth::Two,
                    LigatureSubstFormat1::MIN_SIZE as u32 + new_lig_set_count * 2,
                    false,
                )?;
                new_ligset_table_idx
            } else {
                // move the entire ligature set to the new ligature table
                let lig_set_idx = graph
                    .move_child(
                        this_index,
                        ligset_pos,
                        new_lig_subst_idx,
                        LigatureSubstFormat1::MIN_SIZE as u32 + new_lig_set_count * 2,
                        Offset16::RAW_BYTE_LEN,
                    )?
                    .ok_or(RepackError::GraphErrorInvalidLinkPosition)?;
                compact_lig_set(graph, lig_set_idx)?;
                lig_set_idx
            }
        } else if cur_lig_set_idx == end_lig_set_idx {
            if end_lig_idx == 0 {
                break;
            }
            // This liga set partially overlaps [start, end)
            let num_liga = end_lig_idx;
            let new_ligset_table_idx = create_new_ligature_set(graph, num_liga as u16)?;
            graph.move_children(
                ligset_graph_index,
                LigatureSet::MIN_SIZE as u32,
                new_ligset_table_idx,
                LigatureSet::MIN_SIZE as u32,
                num_liga,
                Offset16::RAW_BYTE_LEN,
            )?;

            // link new ligset
            graph.add_parent_child_link(
                new_lig_subst_idx,
                new_ligset_table_idx,
                LinkWidth::Two,
                LigatureSubstFormat1::MIN_SIZE as u32 + new_lig_set_count * 2,
                false,
            )?;

            new_ligset_table_idx
        } else {
            // This liga set is fully contained within [start, end)
            // We can move the entire ligaset to the new liga subset object.
            graph
                .move_child(
                    this_index,
                    ligset_pos,
                    new_lig_subst_idx,
                    LigatureSubstFormat1::MIN_SIZE as u32 + new_lig_set_count * 2,
                    Offset16::RAW_BYTE_LEN,
                )?
                .ok_or(RepackError::GraphErrorInvalidLinkPosition)?
        };
        new_lig_set_count += 1;

        // The new LigastureSet and all its children need to have a virtual link to the
        // new coverage table
        let mut all_lig_idxes = IntSet::empty();
        find_all_child_idxes(graph, new_ligset_idx, 1, &mut all_lig_idxes)?;
        fix_virtual_links(graph, &all_lig_idxes, coverage_idx, new_coverage_idx)?;
    }

    graph.vertices[new_lig_subst_idx].tail -=
        (lig_set_count - new_lig_set_count as usize) * Offset16::RAW_BYTE_LEN;
    let mut new_lig_subst = LigatureSubstFormat1::from_graph(graph, new_lig_subst_idx)?;
    new_lig_subst.set_format(1);
    new_lig_subst.set_ligset_count(new_lig_set_count as u16);

    let end_glyph = if end_lig_idx == 0 { end_lig_set_idx } else { end_lig_set_idx + 1 };

    make_coverage(
        graph,
        new_coverage_idx,
        cov_glyphs,
        start_lig_set_idx as usize..end_glyph as usize,
    )?;
    Ok(new_lig_subst_idx)
}

// basically shrink the original LigatureSubst table by reducing the num of liga
// sets and coverage glyphs Note additional liga sets and ligatures should have
// been moved to the new LigatureSubst table by clone_range(), Note: We assume
// the input LigatureSubst table(subset output) already contains virtual links
// to Coverage table since we could reuse the existing coverage table obj_idx,
// so no need to clear and reset virtual links
fn shrink(
    graph: &mut Graph,
    table_idx: ObjIdx,
    coverage_idx: ObjIdx,
    cov_glyphs: &[GlyphId],
    shrink_point: u32,
) -> Result<(), RepackError> {
    let end_lig_set_idx = shrink_point >> 16;
    let end_lig_idx = shrink_point & 0xFFFF;
    // adjust the num of ligatures in the last liga set if needed
    if end_lig_idx != 0 {
        let lig_set_idx = graph
            .index_for_position(
                table_idx,
                LigatureSubstFormat1::MIN_SIZE as u32 + end_lig_set_idx * 2,
            )
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?;

        LigatureSet::from_graph(graph, lig_set_idx)?.reset_lig_count(end_lig_idx as u16);
        let lig_set_v =
            graph.mut_vertex(lig_set_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

        let num_liga = lig_set_v.real_links.len();
        // sanity check: all additional links should have been moved to the new table
        // the num of real links left should equal to shrink point index
        if num_liga != end_lig_idx as usize {
            return Err(RepackError::ErrorSplitSubtable);
        }

        lig_set_v.tail = lig_set_v.head + LigatureSet::MIN_SIZE + num_liga * Offset16::RAW_BYTE_LEN;
    }

    // adjust the num of liga sets in LigatureSubst table
    // minus 1:  real link to coverage table
    let num_remaining_liga_set =
        graph.mut_vertex(table_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?.real_links.len()
            - 1;

    let num_liga_set = if end_lig_idx == 0 { end_lig_set_idx } else { end_lig_set_idx + 1 };

    // sanity check
    if num_liga_set as usize != num_remaining_liga_set {
        return Err(RepackError::ErrorSplitSubtable);
    }

    LigatureSubstFormat1::from_graph(graph, table_idx)?.set_ligset_count(num_liga_set as u16);
    let table_v = graph.mut_vertex(table_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
    table_v.tail = table_v.head
        + LigatureSubstFormat1::MIN_SIZE
        + Offset16::RAW_BYTE_LEN * num_remaining_liga_set;

    let coverage_idx = fix_coverage_links(graph, table_idx, coverage_idx)?;
    make_coverage(graph, coverage_idx, cov_glyphs, 0..num_remaining_liga_set)
}

// if coverage is not shared, return original coverage idx
// if coverage is shared:
// 1. clear all virtual links, create a new coverage vertex, add virtual links
//    to it
// 2. add real link to the new coverage vertex and return the new coverage idx
fn fix_coverage_links(
    graph: &mut Graph,
    table_idx: ObjIdx,
    coverage_idx: ObjIdx,
) -> Result<ObjIdx, RepackError> {
    // check if coverage table is shared
    let mut lig_idxes = IntSet::empty();
    find_all_child_idxes(graph, table_idx, 2, &mut lig_idxes)?;

    let coverage_is_shared = graph
        .vertex(coverage_idx)
        .ok_or(RepackError::GraphErrorInvalidObjIndex)?
        .parents
        .keys()
        .any(|p| !lig_idxes.contains(*p as u32));

    if !coverage_is_shared {
        return Ok(coverage_idx);
    }

    let new_coverage_idx = graph.new_vertex(0)?;
    lig_idxes.remove(table_idx as u32);
    lig_idxes.remove(coverage_idx as u32);

    fix_virtual_links(graph, &lig_idxes, coverage_idx, new_coverage_idx)?;
    graph.remap_child(
        table_idx,
        coverage_idx,
        new_coverage_idx,
        LigatureSubstFormat1::COVERAGE_OFFSET_POS as u32,
        false,
    )?;
    Ok(new_coverage_idx)
}

fn create_new_ligature_set(graph: &mut Graph, num_liga: u16) -> Result<ObjIdx, RepackError> {
    let table_size = LigatureSet::MIN_SIZE + num_liga as usize * 2;
    let table_idx = graph.new_vertex(table_size)?;

    let table_head = graph.vertex(table_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?.head;

    graph
        .data
        .get_mut(table_head..table_head + 2)
        .ok_or(RepackError::ErrorReadTable)?
        .copy_from_slice(&num_liga.to_be_bytes());

    Ok(table_idx)
}

fn compact_lig_set(graph: &mut Graph, liga_set_index: ObjIdx) -> Result<(), RepackError> {
    let num_liga = graph
        .vertex(liga_set_index)
        .ok_or(RepackError::GraphErrorInvalidObjIndex)?
        .real_links
        .len();

    let mut lig_set = LigatureSet::from_graph(graph, liga_set_index)?;
    let old_lig_count = lig_set.lig_count() as usize;
    if old_lig_count <= num_liga {
        return Ok(());
    }
    lig_set.reset_lig_count(num_liga as u16);

    let subtract_len = (old_lig_count - num_liga) * Offset16::RAW_BYTE_LEN;

    let lig_set_v = &mut graph.vertices[liga_set_index];
    let start_pos = LigatureSet::MIN_SIZE as u32;
    let mut new_links = Vec::with_capacity(num_liga);
    for i in 0..old_lig_count as u32 {
        let old_pos = start_pos + i * 2;
        let Some((mut pos, mut l)) = lig_set_v.real_links.remove_entry(&old_pos) else {
            continue;
        };

        pos -= subtract_len as u32;
        l.update_position(pos);
        new_links.push((pos, l));
    }

    lig_set_v.real_links.extend(new_links);
    lig_set_v.tail -= subtract_len;
    Ok(())
}

// find all child Ligature/ligatureSet indices
fn find_all_child_idxes(
    graph: &Graph,
    start_idx: ObjIdx,
    depth: u8,
    out: &mut IntSet<u32>,
) -> Result<(), RepackError> {
    if !out.insert(start_idx as u32) {
        return Ok(());
    }

    if depth == 0 {
        return Ok(());
    }

    let v = graph.vertex(start_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
    for l in v.real_links.values() {
        find_all_child_idxes(graph, l.obj_idx(), depth - 1, out)?;
    }
    Ok(())
}

// To make sure coverage table always packed at last(after LigatureSet and
// Ligature tables), add virtual links from the new liga set and all children to
// the new coverage table clear all existing virtual links first
fn fix_virtual_links(
    graph: &mut Graph,
    lig_idxes: &IntSet<u32>,
    old_coverage_idx: ObjIdx,
    coverage_idx: ObjIdx,
) -> Result<(), RepackError> {
    for idx in lig_idxes.iter() {
        let idx = idx as usize;
        let v = graph.mut_vertex(idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

        // sanity check, all virtual links in lig/liga_set idxes should be the same
        // coverage idx
        if !v.virtual_links.iter().all(|l| l.obj_idx() == old_coverage_idx) {
            return Err(RepackError::ErrorSplitSubtable);
        }
        v.virtual_links.clear();
        v.add_link(LinkWidth::default(), coverage_idx, 0, true);

        graph
            .mut_vertex(coverage_idx)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?
            .add_parent(idx, true);
    }

    let old_coverage_v =
        graph.mut_vertex(old_coverage_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

    for idx in lig_idxes.iter() {
        old_coverage_v.remove_parent(idx as usize, true);
    }
    Ok(())
}

struct LigatureSubstFormat1<'a>(DataBytes<'a>);

impl<'a> LigatureSubstFormat1<'a> {
    const MIN_SIZE: usize = 6;
    const FORMAT_BYTE_POS: usize = 0;
    const COVERAGE_OFFSET_POS: usize = 2;
    const LIG_SET_COUNT_POS: usize = 4;

    pub(crate) fn from_graph(graph: &'a mut Graph, obj_idx: ObjIdx) -> Result<Self, RepackError> {
        let data_bytes = DataBytes::from_graph(graph, obj_idx)?;
        let lig_subst = Self(data_bytes);

        if !lig_subst.sanitize() {
            return Err(RepackError::ErrorReadTable);
        }

        Ok(lig_subst)
    }

    fn sanitize(&self) -> bool {
        let len = self.0.len();
        if len < Self::MIN_SIZE {
            return false;
        }

        let lig_set_count = self.lig_set_count();
        len >= Self::MIN_SIZE + (lig_set_count as usize) * Offset16::RAW_BYTE_LEN
    }

    fn lig_set_count(&self) -> u16 {
        self.0.read_at::<u16>(Self::LIG_SET_COUNT_POS)
    }

    fn set_ligset_count(&mut self, lig_set_count: u16) {
        self.0.write_at(lig_set_count, Self::LIG_SET_COUNT_POS);
    }

    fn set_format(&mut self, format: u16) {
        self.0.write_at(format, Self::FORMAT_BYTE_POS);
    }
}

struct LigatureSet<'a>(DataBytes<'a>);
impl<'a> LigatureSet<'a> {
    const MIN_SIZE: usize = 2;
    const LIGATURE_COUNT_POS: usize = 0;

    fn from_graph(graph: &'a mut Graph, obj_idx: ObjIdx) -> Result<Self, RepackError> {
        let data_bytes = DataBytes::from_graph(graph, obj_idx)?;
        let lig_set = Self(data_bytes);

        if !lig_set.sanitize() {
            return Err(RepackError::ErrorReadTable);
        }
        Ok(lig_set)
    }

    fn sanitize(&self) -> bool {
        let len = self.0.len();
        if len < Self::MIN_SIZE {
            return false;
        }

        let lig_count = self.lig_count() as usize;
        len >= Self::MIN_SIZE + lig_count * Offset16::RAW_BYTE_LEN
    }

    fn lig_count(&self) -> u16 {
        self.0.read_at::<u16>(Self::LIGATURE_COUNT_POS)
    }

    fn reset_lig_count(&mut self, lig_count: u16) {
        self.0.write_at(lig_count, Self::LIGATURE_COUNT_POS);
    }
}
