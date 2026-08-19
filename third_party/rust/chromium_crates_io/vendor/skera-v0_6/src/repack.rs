//! Implement repacking algorithm to resolve offset overflows
//! ref:<https://github.com/harfbuzz/harfbuzz/blob/9cfb0e6786ecceabaec7a26fd74b1ddb1209f74d/src/hb-repacker.hh#L454>

use std::cmp::Ordering;

use crate::fnv::FnvHashMap;
use crate::{
    graph::{
        layout::{ExtensionSubtable, Lookup, EXTENSION_TABLE_SIZE},
        ligature_graph::split_ligature_subst,
        markbasepos_graph::split_markbase_pos,
        pairpos_graph::split_pairpos,
        Graph, RepackError,
    },
    serialize::{ObjIdx, Serializer},
};
use write_fonts::{
    read::{
        collections::IntSet,
        tables::{gpos::Gpos, gsub::Gsub},
        TopLevelTable,
    },
    types::{FixedSize, Offset16, Tag},
};

const MAX_ITERATIONS: u16 = 500;
//TODO: add more functionality, serialize output etc.
pub(crate) fn resolve_overflows(
    s: &Serializer,
    tag: Tag,
    max_round: u8,
) -> Result<Vec<u8>, RepackError> {
    let mut graph = Graph::from_serializer(s)?;

    graph.is_fully_connected()?;
    resolve_graph_overflows(&mut graph, tag, max_round, false)?;

    graph
        .serialize()
        .map_err(|_| RepackError::ErrorRepackSerialize)
}

pub(crate) fn resolve_graph_overflows(
    graph: &mut Graph,
    tag: Tag,
    max_round: u8,
    always_recalculate_extensions: bool,
) -> Result<(), RepackError> {
    graph.sort_shortest_distance()?;
    if !graph.has_overflows() {
        return Ok(());
    }

    if tag == Gsub::TAG || tag == Gpos::TAG {
        if always_recalculate_extensions {
            let (lookup_list_idx, lookup_indices) = find_lookup_indices(graph)?;
            let mut visited = FnvHashMap::default();
            presplit_subtables_if_needed(graph, tag, &lookup_indices, &mut visited)?;
            promote_extensions_if_needed(graph, lookup_list_idx, &lookup_indices, tag)?;

            // an additional sorting is needed before assign_spaces (),
            // which requires correct topological ordering to find space roots
            graph.sort_shortest_distance_if_needed()?;
            if graph.overflows().is_empty() {
                return Ok(());
            }
        }
        if graph.assign_spaces()? {
            graph.sort_shortest_distance()?;
        } else {
            graph.sort_shortest_distance_if_needed()?;
        }
    }

    let mut round = 0;
    let mut total_iterations = 0;
    let mut overflows = graph.overflows();
    while !overflows.is_empty() && round < max_round && total_iterations < MAX_ITERATIONS {
        total_iterations += 1;
        if !graph.try_isolating_subgraphs(&overflows)? {
            round += 1;
            if !graph.process_overflows(&overflows)? {
                break;
            }
        }

        graph.sort_shortest_distance()?;
        let _ = std::mem::replace(&mut overflows, graph.overflows());
    }

    //TODO: add more overflow resolution
    if overflows.is_empty() {
        Ok(())
    } else {
        if (tag == Gsub::TAG || tag == Gpos::TAG) && !always_recalculate_extensions {
            return resolve_graph_overflows(graph, tag, max_round, true);
        }
        Err(RepackError::ErrorNoResolution)
    }
}

fn presplit_subtables_if_needed(
    graph: &mut Graph,
    table_tag: Tag,
    lookup_indices: &FnvHashMap<ObjIdx, u32>,
    visited: &mut FnvHashMap<ObjIdx, Vec<ObjIdx>>,
) -> Result<(), RepackError> {
    for lookup_idx in lookup_indices.keys() {
        split_lookup_subtables_if_needed(graph, table_tag, *lookup_idx, visited)?;
    }
    Ok(())
}

fn split_lookup_subtables_if_needed(
    graph: &mut Graph,
    table_tag: Tag,
    lookup_index: ObjIdx,
    visited: &mut FnvHashMap<ObjIdx, Vec<ObjIdx>>,
) -> Result<(), RepackError> {
    let lookup = Lookup::from_graph(graph, lookup_index)?;
    let mut lookup_type = lookup.lookup_type();
    let is_ext = is_extension(lookup_type, table_tag);

    if !is_ext && !splitting_supported_lookup_type(lookup_type, table_tag) {
        return Ok(());
    }

    let num_subtables = lookup.num_subtables();
    let mut ext_lookup_type_checked = false;
    let mut all_subtables = Vec::with_capacity(num_subtables as usize * 2);
    for i in 0..num_subtables as u32 {
        let Some(subtable_idx) = graph.index_for_position(
            lookup_index,
            Lookup::LOOKUP_MIN_SIZE as u32 + i * Offset16::RAW_BYTE_LEN as u32,
        ) else {
            continue;
        };

        if is_ext && !ext_lookup_type_checked {
            let ext_table = ExtensionSubtable::from_graph(graph, subtable_idx)?;
            lookup_type = ext_table.lookup_type();
            if !splitting_supported_lookup_type(lookup_type, table_tag) {
                return Ok(());
            }
            ext_lookup_type_checked = true;
        }

        all_subtables.push(subtable_idx);
        let non_ext_subtable_idx = if is_ext {
            let Some(child_idx) = graph.index_for_position(subtable_idx, 4) else {
                continue;
            };
            child_idx
        } else {
            subtable_idx
        };

        if let Some(new_subtables) = visited.get(&non_ext_subtable_idx) {
            if new_subtables.is_empty() {
                continue;
            }
            all_subtables.extend_from_slice(new_subtables);
            continue;
        }

        // TODO: support more lookup types
        let mut new_subtables = match table_tag {
            Gpos::TAG => {
                if lookup_type == 2 {
                    split_pairpos(graph, non_ext_subtable_idx)?
                } else {
                    split_markbase_pos(graph, non_ext_subtable_idx)?
                }
            }
            Gsub::TAG => split_ligature_subst(graph, non_ext_subtable_idx)?,
            _ => return Err(RepackError::ErrorReadTable),
        };
        if new_subtables.is_empty() {
            visited.insert(non_ext_subtable_idx, new_subtables);
            continue;
        }

        if is_ext {
            graph.add_extension(lookup_type, &mut new_subtables)?;
        }

        all_subtables.extend_from_slice(&new_subtables);
        visited.insert(non_ext_subtable_idx, new_subtables);
    }
    if all_subtables.len() <= num_subtables as usize {
        return Ok(());
    }
    graph.make_lookup(lookup_index, &all_subtables)
}

//TODO: support more lookup types
fn splitting_supported_lookup_type(lookup_type: u16, table_tag: Tag) -> bool {
    match table_tag {
        // GPOS: only support PairPos and MarkBasePos
        Gpos::TAG => lookup_type == 4 || lookup_type == 2,
        // GSUB: currently only support ligature subst
        Gsub::TAG => lookup_type == 4,
        _ => false,
    }
}

fn promote_extensions_if_needed(
    graph: &mut Graph,
    lookup_list_idx: ObjIdx,
    lookups: &FnvHashMap<ObjIdx, u32>,
    table_tag: Tag,
) -> Result<(), RepackError> {
    struct LookupSize {
        obj_idx: ObjIdx,
        lookup_pos: u32,
        lookup_size: usize,
        subgraph_size: usize,
        subtable_count: usize,
        lookup_type: u16,
        is_ext: bool,
    }

    impl LookupSize {
        fn cmp(&self, other: &LookupSize) -> std::cmp::Ordering {
            let bytes_per_subtable_a = self.subtable_count as f64 / self.subgraph_size as f64;
            let bytes_per_subtable_b = other.subtable_count as f64 / other.subgraph_size as f64;
            if bytes_per_subtable_b < bytes_per_subtable_a {
                Ordering::Less
            } else if bytes_per_subtable_b > bytes_per_subtable_a {
                Ordering::Greater
            } else {
                other.lookup_pos.cmp(&self.lookup_pos)
            }
        }
    }
    let mut total_lookup_table_sizes = 0;
    let mut lookup_sizes = Vec::with_capacity(lookups.len());
    let mut visited = IntSet::empty();
    for (lookup_idx, &lookup_pos) in lookups {
        let lookup_v = graph
            .vertex(*lookup_idx)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?;

        let table_size = lookup_v.table_size();
        total_lookup_table_sizes += table_size;

        let lookup = Lookup::from_graph(graph, *lookup_idx)?;
        let lookup_type = lookup.lookup_type();
        let subtable_count = lookup.num_subtables();

        visited.clear();
        let subgraph_size = graph.find_subgraph_size(*lookup_idx, &mut visited, u16::MAX)?;
        lookup_sizes.push(LookupSize {
            obj_idx: *lookup_idx,
            lookup_pos,
            lookup_size: table_size,
            subgraph_size,
            subtable_count: subtable_count as usize,
            lookup_type,
            is_ext: is_extension(lookup_type, table_tag),
        });
    }

    lookup_sizes.sort_by(|a, b| a.cmp(b));

    const MAX_SIZE: usize = u16::MAX as usize;
    let lookup_list_v = graph
        .vertex(lookup_list_idx)
        .ok_or(RepackError::GraphErrorInvalidObjIndex)?;

    let lookup_list_size = lookup_list_v.table_size();
    let l2_l3_size = lookup_list_size + total_lookup_table_sizes; // size of LookupList + lookups
    let mut l3_l4_size = total_lookup_table_sizes; // Lookups + lookup subtables
    let mut l4_plus_size = 0; // subtables and anything below that

    for l in &lookup_sizes {
        let subtables_size = l.subtable_count * EXTENSION_TABLE_SIZE;
        l3_l4_size += subtables_size;
        l4_plus_size += subtables_size;
    }

    let mut layers_full = false;
    let mut idx_map = FnvHashMap::default();
    for l in &lookup_sizes {
        if l.is_ext {
            continue;
        }

        if !layers_full {
            let lookup_size = l.lookup_size;
            visited.clear();
            let subtables_size =
                graph.find_subgraph_size(l.obj_idx, &mut visited, 1)? - lookup_size;
            let remaining_size = l.subgraph_size - subtables_size - lookup_size;

            l3_l4_size += subtables_size;
            l3_l4_size -= l.subtable_count * EXTENSION_TABLE_SIZE;
            l4_plus_size += subtables_size + remaining_size;

            if l2_l3_size < MAX_SIZE && l3_l4_size < MAX_SIZE && l4_plus_size < MAX_SIZE {
                continue;
            }
            layers_full = true;
        }
        graph.make_extension(
            l.obj_idx,
            l.lookup_type,
            extension_type(table_tag),
            &mut idx_map,
        )?;
    }
    Ok(())
}

fn is_extension(lookup_type: u16, table_tag: Tag) -> bool {
    match table_tag {
        Gpos::TAG => lookup_type == 9,
        Gsub::TAG => lookup_type == 7,
        _ => false,
    }
}

fn extension_type(table_tag: Tag) -> Option<u16> {
    match table_tag {
        Gpos::TAG => Some(9),
        Gsub::TAG => Some(7),
        _ => None,
    }
}

fn find_lookup_indices(graph: &Graph) -> Result<(ObjIdx, FnvHashMap<ObjIdx, u32>), RepackError> {
    // pos=8: lookup list position in GSUB/GPOS table
    let lookup_list_idx = graph
        .index_for_position(graph.root_idx(), 8)
        .ok_or(RepackError::GraphErrorInvalidObjIndex)?;

    let lookup_list_v = graph
        .vertex(lookup_list_idx)
        .ok_or(RepackError::GraphErrorInvalidObjIndex)?;
    Ok((lookup_list_idx, lookup_list_v.child_idxes()))
}

#[cfg(test)]
pub(crate) mod test {
    use write_fonts::{read::tables::layout::CoverageTable, types::GlyphId};

    use super::*;
    use crate::{
        graph::test::{
            add_24_offset, add_object, add_offset, add_virtual_offset, add_wide_offset, extend,
            populate_serializer_with_dedup_overflow, populate_serializer_with_overflow,
            start_object,
        },
        Serialize,
    };

    fn populate_serializer_spaces(s: &mut Serializer, with_overflow: bool) {
        let large_string = [b'a'; 70000];
        s.start_serialize().unwrap();

        let obj_i = if with_overflow {
            add_object(s, b"i", 1, false)
        } else {
            0
        };

        // space 2
        let obj_h = add_object(s, b"h", 1, false);
        start_object(s, &large_string, 30000);
        add_offset(s, obj_h);

        let obj_e = s.pop_pack(false).unwrap();
        start_object(s, b"b", 1);
        add_offset(s, obj_e);
        let obj_b = s.pop_pack(false).unwrap();

        // space 1
        let obj_i = if !with_overflow {
            add_object(s, b"i", 1, false)
        } else {
            obj_i
        };

        start_object(s, &large_string, 30000);
        add_offset(s, obj_i);
        let obj_g = s.pop_pack(false).unwrap();

        start_object(s, &large_string, 30000);
        add_offset(s, obj_i);
        let obj_f = s.pop_pack(false).unwrap();

        start_object(s, b"d", 1);
        add_offset(s, obj_g);
        let obj_d = s.pop_pack(false).unwrap();

        start_object(s, b"c", 1);
        add_offset(s, obj_f);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_wide_offset(s, obj_b);
        add_wide_offset(s, obj_c);
        add_wide_offset(s, obj_d);
        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_with_isolation_overflow(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let obj_4 = add_object(s, b"4", 1, false);

        start_object(s, &large_bytes, 60000);
        add_offset(s, obj_4);
        let obj_3 = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 10000);
        add_offset(s, obj_4);
        let obj_2 = s.pop_pack(false).unwrap();

        start_object(s, b"1", 1);
        add_wide_offset(s, obj_3);
        add_offset(s, obj_2);

        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_with_isolation_overflow_complex(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let obj_f = add_object(s, b"f", 1, false);

        start_object(s, b"e", 1);
        add_offset(s, obj_f);
        let obj_e = s.pop_pack(false).unwrap();

        start_object(s, b"c", 1);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, b"d", 1);
        add_offset(s, obj_e);
        let obj_d = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 60000);
        add_offset(s, obj_d);
        let obj_h = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 60000);
        add_offset(s, obj_c);
        add_offset(s, obj_h);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 10000);
        add_offset(s, obj_d);
        let obj_g = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 11000);
        add_offset(s, obj_d);
        let obj_i = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_wide_offset(s, obj_b);
        add_offset(s, obj_g);
        add_offset(s, obj_i);
        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_with_isolation_overflow_complex_expected(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();

        // space 1
        let obj_f_prime = add_object(s, b"f", 1, false);

        start_object(s, b"e", 1);
        add_offset(s, obj_f_prime);
        let obj_e_prime = s.pop_pack(false).unwrap();

        start_object(s, b"d", 1);
        add_offset(s, obj_e_prime);
        let obj_d_prime = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 60000);
        add_offset(s, obj_d_prime);
        let obj_h = s.pop_pack(false).unwrap();

        start_object(s, b"c", 1);
        add_offset(s, obj_e_prime);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 60000);
        add_offset(s, obj_c);
        add_offset(s, obj_h);
        let obj_b = s.pop_pack(false).unwrap();

        // space 0
        let obj_f = add_object(s, b"f", 1, false);

        start_object(s, b"e", 1);
        add_offset(s, obj_f);
        let obj_e = s.pop_pack(false).unwrap();

        start_object(s, b"d", 1);
        add_offset(s, obj_e);
        let obj_d = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 11000);
        add_offset(s, obj_d);
        let obj_i = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 10000);
        add_offset(s, obj_d);
        let obj_g = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_wide_offset(s, obj_b);
        add_offset(s, obj_g);
        add_offset(s, obj_i);
        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_with_isolation_overflow_spaces(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let obj_d = add_object(s, b"f", 1, false);
        let obj_e = add_object(s, b"f", 1, false);

        start_object(s, &large_bytes, 60000);
        add_offset(s, obj_d);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 60000);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_wide_offset(s, obj_b);
        add_wide_offset(s, obj_c);

        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_with_multiple_dedup_overflow(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let leaf = add_object(s, b"def", 3, false);

        const NUM_MIN_NODES: usize = 20;
        let mut mid_nodes = [0; NUM_MIN_NODES];
        for (i, node_obj_idx) in mid_nodes.iter_mut().enumerate() {
            start_object(s, &large_bytes, 10000 + i);
            add_offset(s, leaf);
            *node_obj_idx = s.pop_pack(false).unwrap();
        }

        start_object(s, b"abc", 3);
        for node_obj_idx in mid_nodes {
            add_wide_offset(s, node_obj_idx);
        }
        s.pop_pack(false).unwrap();

        s.end_serialize();
    }

    fn populate_serializer_with_priority_overflow(s: &mut Serializer) {
        let large_bytes = [b'a'; 50000];
        let _ = s.start_serialize();
        let obj_e = add_object(s, b"e", 1, false);
        let obj_d = add_object(s, b"d", 1, false);

        start_object(s, &large_bytes, 50000);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 20000);
        add_offset(s, obj_d);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_offset(s, obj_b);
        add_offset(s, obj_c);
        s.pop_pack(false);

        s.end_serialize();
    }

    fn populate_serializer_with_priority_overflow_expected(s: &mut Serializer) {
        let large_bytes = [b'a'; 50000];
        let _ = s.start_serialize();
        let obj_e = add_object(s, b"e", 1, false);

        start_object(s, &large_bytes, 50000);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        let obj_d = add_object(s, b"d", 1, false);

        start_object(s, &large_bytes, 20000);
        add_offset(s, obj_d);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_offset(s, obj_b);
        add_offset(s, obj_c);
        s.pop_pack(false).unwrap();

        s.end_serialize();
    }

    fn populate_serializer_spaces_16bit_connection(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let obj_g = add_object(s, b"g", 1, false);
        let obj_h = add_object(s, b"h", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_g);
        let obj_e = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_h);
        let obj_f = s.pop_pack(false).unwrap();

        start_object(s, b"c", 1);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, b"d", 1);
        add_offset(s, obj_f);
        let obj_d = s.pop_pack(false).unwrap();

        start_object(s, b"b", 1);
        add_offset(s, obj_e);
        add_offset(s, obj_h);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_offset(s, obj_b);
        add_wide_offset(s, obj_c);
        add_wide_offset(s, obj_d);

        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_spaces_16bit_connection_expected(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let obj_g_prime = add_object(s, b"g", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_g_prime);
        let obj_e_prime = s.pop_pack(false).unwrap();

        start_object(s, b"c", 1);
        add_offset(s, obj_e_prime);
        let obj_c = s.pop_pack(false).unwrap();

        let obj_h_prime = add_object(s, b"h", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_h_prime);
        let obj_f = s.pop_pack(false).unwrap();

        start_object(s, b"d", 1);
        add_offset(s, obj_f);
        let obj_d = s.pop_pack(false).unwrap();

        let obj_g = add_object(s, b"g", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_g);
        let obj_e = s.pop_pack(false).unwrap();

        let obj_h = add_object(s, b"h", 1, false);

        start_object(s, b"b", 1);
        add_offset(s, obj_e);
        add_offset(s, obj_h);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_offset(s, obj_b);
        add_wide_offset(s, obj_c);
        add_wide_offset(s, obj_d);

        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_short_and_wide_subgraph_root(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let obj_e = add_object(s, b"e", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_c);
        let obj_d = s.pop_pack(false).unwrap();

        start_object(s, b"b", 1);
        add_offset(s, obj_c);
        add_offset(s, obj_e);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_offset(s, obj_b);
        add_wide_offset(s, obj_c);
        add_wide_offset(s, obj_d);

        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_short_and_wide_subgraph_root_expected(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let obj_e_prime = add_object(s, b"e", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_e_prime);
        let obj_c_prime = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_c_prime);
        let obj_d = s.pop_pack(false).unwrap();

        let obj_e = add_object(s, b"e", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, b"b", 1);
        add_offset(s, obj_c);
        add_offset(s, obj_e);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_offset(s, obj_b);
        add_wide_offset(s, obj_c_prime);
        add_wide_offset(s, obj_d);

        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_with_split_spaces(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let obj_f = add_object(s, b"f", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_f);
        let obj_d = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_f);
        let obj_e = s.pop_pack(false).unwrap();

        start_object(s, b"b", 1);
        add_offset(s, obj_d);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, b"c", 1);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_wide_offset(s, obj_b);
        add_wide_offset(s, obj_c);

        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_with_split_spaces_expected(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let obj_f_prime = add_object(s, b"f", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_f_prime);
        let obj_d = s.pop_pack(false).unwrap();

        start_object(s, b"b", 1);
        add_offset(s, obj_d);
        let obj_b = s.pop_pack(false).unwrap();

        let obj_f = add_object(s, b"f", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_f);
        let obj_e = s.pop_pack(false).unwrap();

        start_object(s, b"c", 1);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_wide_offset(s, obj_b);
        add_wide_offset(s, obj_c);

        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_with_split_spaces_2(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let obj_f = add_object(s, b"f", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_f);
        let obj_d = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_f);
        let obj_e = s.pop_pack(false).unwrap();

        start_object(s, b"b", 1);
        add_offset(s, obj_d);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, b"c", 1);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_offset(s, obj_b);
        add_wide_offset(s, obj_b);
        add_wide_offset(s, obj_c);

        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_with_split_spaces_expected_2(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();

        // space 2
        let obj_f_double_prime = add_object(s, b"f", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_f_double_prime);
        let obj_d_prime = s.pop_pack(false).unwrap();

        start_object(s, b"b", 1);
        add_offset(s, obj_d_prime);
        let obj_b_prime = s.pop_pack(false).unwrap();

        // space 1
        let obj_f_prime = add_object(s, b"f", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_f_prime);
        let obj_e = s.pop_pack(false).unwrap();

        start_object(s, b"c", 1);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        // space 0
        let obj_f = add_object(s, b"f", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_f);
        let obj_d = s.pop_pack(false).unwrap();

        start_object(s, b"b", 1);
        add_offset(s, obj_d);
        let obj_b = s.pop_pack(false).unwrap();

        // Root
        start_object(s, b"a", 1);
        add_offset(s, obj_b);
        add_wide_offset(s, obj_b_prime);
        add_wide_offset(s, obj_c);

        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_with_24_and_32_bit_offsets(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();

        let obj_f = add_object(s, b"f", 1, false);
        let obj_g = add_object(s, b"g", 1, false);
        let obj_j = add_object(s, b"j", 1, false);
        let obj_k = add_object(s, b"k", 1, false);

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_f);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_g);
        let obj_d = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_j);
        let obj_h = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 40000);
        add_offset(s, obj_k);
        let obj_i = s.pop_pack(false).unwrap();

        start_object(s, b"e", 1);
        add_wide_offset(s, obj_h);
        add_wide_offset(s, obj_i);
        let obj_e = s.pop_pack(false).unwrap();

        start_object(s, b"b", 1);
        add_24_offset(s, obj_c);
        add_24_offset(s, obj_d);
        add_24_offset(s, obj_e);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_24_offset(s, obj_b);

        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_virtual_link(s: &mut Serializer) {
        let _ = s.start_serialize();
        let obj_d = add_object(s, b"d", 1, false);

        start_object(s, b"b", 1);
        add_offset(s, obj_d);
        let obj_b = s.pop_pack(false).unwrap();

        start_object(s, b"e", 1);
        add_virtual_offset(s, obj_b);
        let obj_e = s.pop_pack(false).unwrap();

        start_object(s, b"c", 1);
        add_offset(s, obj_e);
        let obj_c = s.pop_pack(false).unwrap();

        start_object(s, b"a", 1);
        add_offset(s, obj_b);
        add_offset(s, obj_c);
        s.pop_pack(false).unwrap();

        s.end_serialize();
    }

    fn add_gsub_gpos_header(s: &mut Serializer, lookup_list_idx: ObjIdx) -> ObjIdx {
        let header_bytes = [0, 1, 0, 0, 0, 0, 0, 0];
        start_object(s, &header_bytes, 8);
        add_offset(s, lookup_list_idx);
        s.pop_pack(false).unwrap()
    }

    fn add_lookup_list(s: &mut Serializer, lookup_count: usize, lookups: &[ObjIdx]) -> ObjIdx {
        let lookup_count_bytes = [0, lookup_count as u8];
        start_object(s, &lookup_count_bytes, 2);

        for i in lookups.iter().take(lookup_count) {
            add_offset(s, *i);
        }

        s.pop_pack(false).unwrap()
    }

    fn add_extension(s: &mut Serializer, child_idx: ObjIdx, ext_type: u8, shared: bool) -> ObjIdx {
        let ext_header_bytes = [0, 1, 0, ext_type];
        start_object(s, &ext_header_bytes, 4);
        add_wide_offset(s, child_idx);
        s.pop_pack(shared).unwrap()
    }

    fn start_lookup(s: &mut Serializer, lookup_type: u8, num_subtables: u8) {
        let lookup_bytes = [0, lookup_type, 0, 16, 0, num_subtables];
        start_object(s, &lookup_bytes, 6);
    }

    fn finish_lookup(s: &mut Serializer) -> ObjIdx {
        let filter = [0, 0];
        s.embed_bytes(&filter).unwrap();
        s.pop_pack(false).unwrap()
    }

    // Adds coverage table fro [start, end]
    fn add_coverage(s: &mut Serializer, start: u16, end: u16, shared: bool) -> ObjIdx {
        let start_be_bytes = start.to_be_bytes();
        let end_be_bytes = end.to_be_bytes();
        match end - start {
            0 => {
                let coverage: [u8; 6] = [0, 1, 0, 1, start_be_bytes[0], start_be_bytes[1]];
                add_object(s, &coverage, 6, shared)
            }
            1 => {
                let coverage: [u8; 8] = [
                    0,
                    1,
                    0,
                    2,
                    start_be_bytes[0],
                    start_be_bytes[1],
                    end_be_bytes[0],
                    end_be_bytes[1],
                ];
                add_object(s, &coverage, 8, shared)
            }
            _ => {
                let coverage: [u8; 10] = [
                    0,
                    2,
                    0,
                    1,
                    start_be_bytes[0],
                    start_be_bytes[1],
                    end_be_bytes[0],
                    end_be_bytes[1],
                    0,
                    0,
                ];
                add_object(s, &coverage, 10, shared)
            }
        }
    }

    fn add_coverage_from_glyphs(s: &mut Serializer, glyphs: &[GlyphId]) -> ObjIdx {
        s.push().unwrap();
        CoverageTable::serialize(s, glyphs).unwrap();
        s.pop_pack(false).unwrap()
    }

    fn add_liga_set_header(s: &mut Serializer, liga_count: u16) {
        let liga_count_bytes = liga_count.to_be_bytes();
        start_object(s, &liga_count_bytes, 2);
    }

    fn add_liga_header(s: &mut Serializer, liga_set_count: u16, coverage_idx: ObjIdx) {
        start_object(s, &1_u16.to_be_bytes(), 2);
        add_offset(s, coverage_idx);
        let liga_set_count_bytes = liga_set_count.to_be_bytes();
        s.embed_bytes(&liga_set_count_bytes).unwrap();
    }

    fn populate_serializer_with_extension_promotion(
        s: &mut Serializer,
        num_extensions: usize,
        shared_subtables: bool,
    ) {
        const NUM_LOOKUPS: usize = 5;
        const NUM_SUBTABLES: usize = NUM_LOOKUPS * 2;
        let mut lookups = [0; NUM_LOOKUPS];
        let mut subtables = [0; NUM_SUBTABLES];
        let mut extensions = [0; NUM_SUBTABLES];

        let large_bytes = [b'a'; 60000];
        s.start_serialize().unwrap();

        for i in (0..NUM_SUBTABLES).rev() {
            subtables[i] = add_object(s, &large_bytes, 15000 + i, false);
        }

        assert!(NUM_LOOKUPS >= num_extensions);
        for i in ((NUM_LOOKUPS - num_extensions) * 2..NUM_SUBTABLES).rev() {
            extensions[i] = add_extension(s, subtables[i], 5, false);
        }

        for i in (0..NUM_LOOKUPS).rev() {
            let is_ext = i >= (NUM_LOOKUPS - num_extensions);
            let lookup_type = if is_ext { 7 } else { 5 };
            let num_subtables = if shared_subtables && i > 2 { 3 } else { 2 };
            start_lookup(s, lookup_type, num_subtables);

            if is_ext {
                if shared_subtables && i > 2 {
                    add_offset(s, extensions[i * 2 - 1]);
                }
                add_offset(s, extensions[i * 2]);
                add_offset(s, extensions[i * 2 + 1]);
            } else {
                if shared_subtables && i > 2 {
                    add_offset(s, subtables[i * 2 - 1]);
                }
                add_offset(s, subtables[i * 2]);
                add_offset(s, subtables[i * 2 + 1]);
            }

            lookups[i] = finish_lookup(s);
        }

        let lookup_list_idx = add_lookup_list(s, NUM_LOOKUPS, &lookups);
        add_gsub_gpos_header(s, lookup_list_idx);
        s.end_serialize();
    }

    fn populate_serializer_with_large_mark_base_pos_1(
        s: &mut Serializer,
        mark_count: usize,
        class_count: usize,
        base_count: usize,
        table_count: usize,
    ) {
        fn populate_mark_base_pos_buffers(
            mark_count: usize,
            class_count: usize,
            base_count: usize,
            s: &mut Serializer,
        ) -> (Vec<ObjIdx>, Vec<ObjIdx>) {
            let num_base_anchors = base_count * class_count;
            let mut base_anchors = Vec::with_capacity(num_base_anchors);
            let mut mark_anchors = Vec::with_capacity(mark_count);
            let mut anchor_buffers = Vec::with_capacity(num_base_anchors + 100);

            for i in 0..(num_base_anchors / 2 + 50) as u16 {
                anchor_buffers.extend_from_slice(&i.to_be_bytes());
            }

            for i in 0..num_base_anchors {
                let anchor_idx = add_object(s, &anchor_buffers[i..], 100, false);
                base_anchors.push(anchor_idx);
            }

            for i in 0..mark_count {
                let anchor_idx = add_object(s, &anchor_buffers[i..], 4, false);
                mark_anchors.push(anchor_idx);
            }
            (base_anchors, mark_anchors)
        }

        fn create_mark_base_pos_1(
            s: &mut Serializer,
            table_index: usize,
            base_anchors: &[ObjIdx],
            mark_anchors: &[ObjIdx],
            class_per_table: usize,
            class_count: usize,
        ) -> ObjIdx {
            let mark_count = mark_anchors.len();
            let base_count = base_anchors.len() / class_count;

            let mark_per_class = mark_count / class_count;
            let start_class = class_per_table * table_index;
            let end_class = class_per_table * (table_index + 1) - 1;

            // baseArray
            start_object(s, &(base_count as u16).to_be_bytes(), 2);

            for base in 0..base_count {
                for class in start_class..=end_class {
                    let i = base * class_count + class;
                    add_offset(s, base_anchors[i]);
                }
            }

            let base_array = s.pop_pack(false).unwrap();

            //markArray
            let num_marks = class_per_table * mark_per_class;
            start_object(s, &(num_marks as u16).to_be_bytes(), 2);

            let mut mark_cov_glyphs = Vec::with_capacity(mark_count);
            for (mark, anchor_idx) in mark_anchors.iter().enumerate() {
                let mut class = mark % class_count;
                if class < start_class || class > end_class {
                    continue;
                }
                class -= start_class;
                s.embed(class as u16).unwrap();
                add_offset(s, *anchor_idx);
                mark_cov_glyphs.push(GlyphId::from(mark as u32));
            }
            let mark_array = s.pop_pack(false).unwrap();

            // mark Coverage
            let mark_coverage = add_coverage_from_glyphs(s, &mark_cov_glyphs);

            // base Coverage
            let base_coverage = add_coverage(s, 10, 10 + base_count as u16 - 1, false);

            // header: format
            start_object(s, &1_u16.to_be_bytes(), 2);

            add_offset(s, mark_coverage);
            add_offset(s, base_coverage);

            s.embed(class_per_table as u16).unwrap();
            add_offset(s, mark_array);
            add_offset(s, base_array);
            s.pop_pack(false).unwrap()
        }

        s.start_serialize().unwrap();
        let (base_anchors, mark_anchnors) =
            populate_mark_base_pos_buffers(mark_count, class_count, base_count, s);
        let mut mark_base_pos_idxes = vec![0; table_count];
        let class_per_table = class_count / table_count;
        for (i, idx) in mark_base_pos_idxes.iter_mut().enumerate() {
            *idx = create_mark_base_pos_1(
                s,
                i,
                &base_anchors,
                &mark_anchnors,
                class_per_table,
                class_count,
            );
        }

        for idx in mark_base_pos_idxes.iter_mut() {
            *idx = add_extension(s, *idx, 4, false);
        }

        start_lookup(s, 9, table_count as u8);
        for idx in mark_base_pos_idxes {
            add_offset(s, idx);
        }

        let lookup_idx = finish_lookup(s);
        let lookups = [lookup_idx; 1];
        let lookup_list_idx = add_lookup_list(s, 1, &lookups);
        add_gsub_gpos_header(s, lookup_list_idx);
        s.end_serialize();
    }

    #[allow(clippy::too_many_arguments)]
    fn populate_serializer_with_large_ligsubst(
        s: &mut Serializer,
        lig_subst_count: usize,
        liga_set_count: usize,
        liga_per_set_count: usize,
        liga_size: usize,
        sequential_liga_sets: bool,
        shared: bool,
        liga_subst_idxes: &mut [ObjIdx],
        unique_lig_str: bool,
        shared_extension: bool,
    ) {
        let mut liga = vec![0_usize; liga_set_count * liga_per_set_count];
        let mut liga_set = vec![0_usize; liga_set_count];
        let mut ch = b'a';
        for (l, subst_idx) in liga_subst_idxes
            .iter_mut()
            .enumerate()
            .take(lig_subst_count)
        {
            let coverage_start = if sequential_liga_sets {
                l * liga_set_count
            } else {
                0
            };

            let coverage_end = if sequential_liga_sets {
                (l + 1) * liga_set_count - 1
            } else {
                liga_set_count - 1
            };

            let coverage_idx = add_coverage(s, coverage_start as u16, coverage_end as u16, shared);
            for i in 0..liga_set_count {
                for j in 0..liga_per_set_count {
                    let large_str = [ch; 100000];
                    start_object(s, &large_str, liga_size);
                    if unique_lig_str {
                        ch += 1;
                    }

                    add_virtual_offset(s, coverage_idx);
                    liga[i * liga_per_set_count + j] = s.pop_pack(shared).unwrap();
                }
                add_liga_set_header(s, liga_per_set_count as u16);
                add_virtual_offset(s, coverage_idx);

                for j in 0..liga_per_set_count {
                    add_offset(s, liga[i * liga_per_set_count + j]);
                }
                liga_set[i] = s.pop_pack(shared).unwrap();
            }

            add_liga_header(s, liga_set_count as u16, coverage_idx);
            for lig_set_idx in liga_set.iter().take(liga_set_count) {
                add_offset(s, *lig_set_idx);
            }

            *subst_idx = s.pop_pack(shared).unwrap();
        }

        for subst_idx in liga_subst_idxes.iter_mut().take(lig_subst_count) {
            *subst_idx = add_extension(s, *subst_idx, 4, shared_extension);
        }
    }

    fn populate_serializer_with_large_liga(
        s: &mut Serializer,
        lig_subst_count: usize,
        liga_set_count: usize,
        liga_per_set_count: usize,
        liga_size: usize,
        sequential_liga_sets: bool,
    ) {
        s.start_serialize().unwrap();
        let mut liga_subst_idxes = vec![0; lig_subst_count];
        populate_serializer_with_large_ligsubst(
            s,
            lig_subst_count,
            liga_set_count,
            liga_per_set_count,
            liga_size,
            sequential_liga_sets,
            false,
            &mut liga_subst_idxes,
            false,
            false,
        );

        start_lookup(s, 7, lig_subst_count as u8);
        for subst_idx in liga_subst_idxes.iter().take(lig_subst_count) {
            add_offset(s, *subst_idx);
        }

        let mut lookups = [0; 1];
        lookups[0] = finish_lookup(s);
        let lookup_list_idx = add_lookup_list(s, 1, &lookups);
        add_gsub_gpos_header(s, lookup_list_idx);
        s.end_serialize();
    }

    fn populate_serializer_with_large_liga_overlapping_clone_result(s: &mut Serializer) {
        s.start_serialize().unwrap();

        const LIGA_SIZE: usize = 30000;
        let large_bytes = [b'a'; 100000];

        let mut liga = [0_usize; 2];
        let mut liga_subst = [0_usize; 3];
        let mut liga_set = [0_usize; 2];

        // LigSubst 3
        let coverage = add_coverage(s, 1, 1, false);
        for i in (0..2_usize).rev() {
            start_object(s, &large_bytes, LIGA_SIZE);
            add_virtual_offset(s, coverage);
            liga[i] = s.pop_pack(false).unwrap();
        }

        add_liga_set_header(s, 2);
        add_virtual_offset(s, coverage);
        for liga_idx in liga {
            add_offset(s, liga_idx);
        }
        liga_set[0] = s.pop_pack(false).unwrap();

        add_liga_header(s, 1, coverage);
        add_offset(s, liga_set[0]);
        liga_subst[2] = s.pop_pack(false).unwrap();

        // LigSubst 2
        let coverage = add_coverage(s, 0, 1, false);
        for i in (0..2_usize).rev() {
            start_object(s, &large_bytes, LIGA_SIZE);
            add_virtual_offset(s, coverage);
            liga[i] = s.pop_pack(false).unwrap();
        }

        add_liga_set_header(s, 1);
        add_virtual_offset(s, coverage);
        add_offset(s, liga[1]);
        liga_set[1] = s.pop_pack(false).unwrap();

        add_liga_set_header(s, 1);
        add_virtual_offset(s, coverage);
        add_offset(s, liga[0]);
        liga_set[0] = s.pop_pack(false).unwrap();

        add_liga_header(s, 2, coverage);
        add_offset(s, liga_set[0]);
        add_offset(s, liga_set[1]);
        liga_subst[1] = s.pop_pack(false).unwrap();

        // LigSubst 1
        let coverage = add_coverage(s, 0, 0, false);
        for i in (0..2_usize).rev() {
            start_object(s, &large_bytes, LIGA_SIZE);
            add_virtual_offset(s, coverage);
            liga[i] = s.pop_pack(false).unwrap();
        }

        add_liga_set_header(s, 2);
        add_virtual_offset(s, coverage);
        for liga_idx in liga {
            add_offset(s, liga_idx);
        }
        liga_set[0] = s.pop_pack(false).unwrap();

        add_liga_header(s, 1, coverage);
        add_offset(s, liga_set[0]);
        liga_subst[0] = s.pop_pack(false).unwrap();

        for l in liga_subst.iter_mut().rev() {
            *l = add_extension(s, *l, 4, false);
        }

        start_lookup(s, 7, 3);
        for l in &liga_subst {
            add_offset(s, *l);
        }

        let mut lookups = [0_usize; 1];
        lookups[0] = finish_lookup(s);
        let lookup_list_idx = add_lookup_list(s, 1, &lookups);
        add_gsub_gpos_header(s, lookup_list_idx);
        s.end_serialize();
    }

    fn populate_serializer_with_shared_large_liga(
        s: &mut Serializer,
        lig_subst_count: usize,
        liga_set_count: usize,
        liga_per_set_count: usize,
        liga_size: usize,
    ) {
        s.start_serialize().unwrap();
        let mut lookups: [ObjIdx; 2] = [0; 2];
        // Lookup
        // LigSubst: shared with another Lookup table, needs splitting
        let mut liga_subst_idxes = vec![0; lig_subst_count + 1];
        populate_serializer_with_large_ligsubst(
            s,
            lig_subst_count,
            liga_set_count,
            liga_per_set_count,
            liga_size,
            true,
            true,
            &mut liga_subst_idxes,
            true,
            true,
        );

        start_lookup(s, 7, lig_subst_count as u8);
        for l in liga_subst_idxes.iter().take(lig_subst_count) {
            add_offset(s, *l);
        }
        lookups[0] = finish_lookup(s);

        // Lookup with 2 LigSubst tables
        // LigSubst: small one, not shared, no split
        populate_serializer_with_large_ligsubst(
            s,
            1,
            1,
            1,
            10,
            true,
            false,
            &mut liga_subst_idxes,
            false,
            false,
        );

        // LigSubst: shared, needs splitting
        let lig_subst_idxes = liga_subst_idxes.get_mut(1..=lig_subst_count).unwrap();
        populate_serializer_with_large_ligsubst(
            s,
            lig_subst_count,
            liga_set_count,
            liga_per_set_count,
            liga_size,
            true,
            true,
            lig_subst_idxes,
            true,
            true,
        );

        start_lookup(s, 7, lig_subst_count as u8 + 1);
        for l in liga_subst_idxes {
            add_offset(s, l);
        }
        lookups[1] = finish_lookup(s);

        let lookup_list_idx = add_lookup_list(s, 2, &lookups);
        add_gsub_gpos_header(s, lookup_list_idx);
        s.end_serialize();
    }

    fn populate_serializer_with_liga_shared_coverage(
        s: &mut Serializer,
        lig_subst_count: usize,
        liga_set_count: usize,
        liga_per_set_count: usize,
        liga_size: usize,
    ) {
        s.start_serialize().unwrap();
        let mut liga_subst = vec![0_usize; lig_subst_count + 1];
        // LigSubst: small one, no split, coverage shared
        populate_serializer_with_large_ligsubst(
            s,
            1,
            6,
            2,
            10,
            true,
            true,
            &mut liga_subst,
            true,
            true,
        );
        // LigSubst: shared coverage, needs splitting
        let lig_subst_idxes = liga_subst.get_mut(1..=lig_subst_count).unwrap();
        populate_serializer_with_large_ligsubst(
            s,
            lig_subst_count,
            liga_set_count,
            liga_per_set_count,
            liga_size,
            true,
            true,
            lig_subst_idxes,
            true,
            true,
        );

        start_lookup(s, 7, lig_subst_count as u8 + 1);
        for l in liga_subst {
            add_offset(s, l);
        }

        let mut lookups = [0; 1];
        lookups[0] = finish_lookup(s);

        let lookup_list_idx = add_lookup_list(s, 1, &lookups);
        add_gsub_gpos_header(s, lookup_list_idx);
        s.end_serialize();
    }

    fn add_pair_pos_1(s: &mut Serializer, pair_sets: &[ObjIdx], coverage: ObjIdx) -> ObjIdx {
        let format = 1_u16;
        start_object(s, &format.to_be_bytes(), 2);
        add_offset(s, coverage);

        let count = pair_sets.len() as u16;
        s.embed(0_u16).unwrap(); // ValueFormat1
        s.embed(0_u16).unwrap(); // ValueFormat2
        s.embed(count).unwrap(); // PairSetCount

        for &pair_set in pair_sets {
            add_offset(s, pair_set);
        }

        s.pop_pack(false).unwrap()
    }

    fn populate_serializer_with_large_pair_pos_1(
        s: &mut Serializer,
        num_pair_pos_1: usize,
        num_pair_set: usize,
        as_extension: bool,
    ) {
        let large_bytes = [b'a'; 60000];
        s.start_serialize().unwrap();

        let total_pair_set = num_pair_pos_1 * num_pair_set;
        let mut pair_set = vec![0; total_pair_set];
        let mut coverage = vec![0; num_pair_pos_1];
        let mut pair_pos_1 = vec![0; num_pair_pos_1];

        for i in (0..num_pair_pos_1).rev() {
            for j in (i * num_pair_set..(i + 1) * num_pair_set).rev() {
                pair_set[j] = add_object(s, &large_bytes, 30000 + j, false);
            }

            coverage[i] = add_coverage(
                s,
                (i * num_pair_set) as u16,
                ((i + 1) * num_pair_set - 1) as u16,
                false,
            );

            let start_set = i * num_pair_set;
            let end_set = (i + 1) * num_pair_set;
            pair_pos_1[i] = add_pair_pos_1(s, &pair_set[start_set..end_set], coverage[i]);
        }

        let mut pair_pos_2 = add_object(s, &large_bytes, 200, false);

        if as_extension {
            pair_pos_2 = add_extension(s, pair_pos_2, 2, false);
            for p in pair_pos_1.iter_mut().rev() {
                *p = add_extension(s, *p, 2, false);
            }
        }

        let lookup_type = if as_extension { 9 } else { 2 };
        start_lookup(s, lookup_type, (1 + num_pair_pos_1) as u8);

        for &p in &pair_pos_1 {
            add_offset(s, p);
        }
        add_offset(s, pair_pos_2);

        let lookup = finish_lookup(s);
        let lookups = [lookup; 1];
        let lookup_list = add_lookup_list(s, 1, &lookups);

        add_gsub_gpos_header(s, lookup_list);

        s.end_serialize();
    }

    fn add_class_def(s: &mut Serializer, start_glyph: u16, end_glyph: u16) -> ObjIdx {
        let count = end_glyph - start_glyph;
        let header = [
            0,
            1,
            (start_glyph >> 8) as u8,
            (start_glyph & 0xFF) as u8,
            (count >> 8) as u8,
            (count & 0xFF) as u8,
        ];

        start_object(s, &header, 6);
        for i in 1..=count {
            s.embed_bytes(&i.to_be_bytes()).unwrap();
        }

        s.pop_pack(false).unwrap()
    }

    #[allow(clippy::too_many_arguments)]
    fn add_pair_pos_2(
        s: &mut Serializer,
        starting_class: u16,
        coverage: ObjIdx,
        class_def_1: ObjIdx,
        class_1_count: u16,
        class_def_2: ObjIdx,
        class_2_count: u16,
        device_tables: Option<&[ObjIdx]>,
    ) -> ObjIdx {
        let format = 2_u16;
        start_object(s, &format.to_be_bytes(), 2);
        add_offset(s, coverage);

        let format1: u16 = 0x01 | 0x02 | 0x08; // XPlacement, YPlacement, YAdvance
        let mut format2: u16 = 0x04; // XAdvance
        if device_tables.is_some() {
            format2 |= 0x20; // YAdvDevice
        }

        s.embed(format1).unwrap();
        s.embed(format2).unwrap();

        add_offset(s, class_def_1);
        add_offset(s, class_def_2);

        s.embed(class_1_count).unwrap();
        s.embed(class_2_count).unwrap();

        let mut device_index = 0;
        for i in 0..class_1_count {
            for _ in 0..class_2_count {
                for _ in 0..4 {
                    let val = i + starting_class;
                    let bytes = [val as u8, val as u8];
                    s.embed_bytes(&bytes).unwrap();
                }

                if let Some(devices) = device_tables {
                    add_offset(s, devices[device_index]);
                    device_index += 1;
                }
            }
        }

        s.pop_pack(false).unwrap()
    }

    fn populate_serializer_with_large_pair_pos_2(
        s: &mut Serializer,
        num_pair_pos_2: usize,
        num_class_1: usize,
        num_class_2: usize,
        as_extension: bool,
        with_device_tables: bool,
        extra_table: bool,
    ) {
        s.start_serialize().unwrap();

        let mut class_def_1 = vec![0; num_pair_pos_2];
        let mut class_def_2 = vec![0; num_pair_pos_2];
        let mut coverage = vec![0; num_pair_pos_2];
        let mut pair_pos_2 = vec![0; num_pair_pos_2];

        let mut device_tables = vec![0; num_pair_pos_2 * num_class_1 * num_class_2];

        for i in (0..num_pair_pos_2).rev() {
            let start_glyph = (5 + i * num_class_1) as u16;

            if num_class_2 >= num_class_1 {
                class_def_2[i] = add_class_def(s, 11, (10 + num_class_2) as u16);
                class_def_1[i] =
                    add_class_def(s, start_glyph + 1, start_glyph + num_class_1 as u16);
            } else {
                class_def_1[i] =
                    add_class_def(s, start_glyph + 1, start_glyph + num_class_1 as u16);
                class_def_2[i] = add_class_def(s, 11, (10 + num_class_2) as u16);
            }

            coverage[i] = add_coverage(s, start_glyph, start_glyph + num_class_1 as u16 - 1, false);

            if with_device_tables {
                let start_idx = i * num_class_1 * num_class_2;
                let end_idx = (i + 1) * num_class_1 * num_class_2;
                for j in (start_idx..end_idx).rev() {
                    let bytes = [((j >> 8) & 0xFF) as u8, (j & 0xFF) as u8];
                    device_tables[j] = add_object(s, &bytes, 2, false);
                }
            }

            pair_pos_2[i] = add_pair_pos_2(
                s,
                (1 + i * num_class_1) as u16,
                coverage[i],
                class_def_1[i],
                num_class_1 as u16,
                class_def_2[i],
                num_class_2 as u16,
                if with_device_tables {
                    Some(&device_tables[i * num_class_1 * num_class_2..])
                } else {
                    None
                },
            );
        }
        let mut pair_pos_1 = 0;
        if extra_table {
            let large_bytes = vec![b'a'; 100000];
            pair_pos_1 = add_object(s, &large_bytes, 100000, false);
        }

        if as_extension {
            for i in (0..num_pair_pos_2).rev() {
                pair_pos_2[i] = add_extension(s, pair_pos_2[i], 2, false);
            }
            if extra_table {
                pair_pos_1 = add_extension(s, pair_pos_1, 2, false);
            }
        }

        let lookup_type = if as_extension { 9 } else { 2 };
        let num_subtables = if extra_table {
            num_pair_pos_2 + 1
        } else {
            num_pair_pos_2
        } as u8;
        start_lookup(s, lookup_type, num_subtables);

        if extra_table {
            add_offset(s, pair_pos_1);
        }

        for &i in pair_pos_2.iter().take(num_pair_pos_2) {
            add_offset(s, i);
        }

        let lookup = finish_lookup(s);

        let lookups = [lookup; 1];
        let lookup_list = add_lookup_list(s, 1, &lookups);
        add_gsub_gpos_header(s, lookup_list);

        s.end_serialize();
    }

    fn populate_serializer_with_repack_last(s: &mut Serializer, with_overflow: bool) {
        let large_bytes = [b'c'; 70000];
        s.start_serialize().unwrap();
        s.push().unwrap();

        // Obj E
        let mut obj_e_1 = if with_overflow {
            add_object(s, b"a", 1, false)
        } else {
            0
        };

        let obj_e_2 = if with_overflow {
            obj_e_1
        } else {
            add_object(s, b"a", 1, false)
        };

        // Obj D
        s.push().unwrap();
        add_offset(s, obj_e_2);
        extend(s, &large_bytes, 30000);
        let obj_d = s.pop_pack(false).unwrap();
        add_offset(s, obj_d);
        assert_eq!(s.last_added_child_index().unwrap(), obj_d);
        if !with_overflow {
            obj_e_1 = add_object(s, b"a", 1, false);
        }

        // Obj C
        s.push().unwrap();
        add_offset(s, obj_e_1);
        extend(s, &large_bytes, 40000);
        let obj_c = s.pop_pack(false).unwrap();
        add_offset(s, obj_c);

        // Obj B
        let obj_b = add_object(s, b"b", 1, false);
        add_offset(s, obj_b);

        //Obj A
        s.repack_last(obj_d).unwrap();
        s.pop_pack(false).unwrap();
        s.end_serialize();
    }

    fn populate_serializer_virtual(s: &mut Serializer, with_overflow: bool) {
        let large_bytes = [b'a'; 50000];
        s.start_serialize().unwrap();

        let obj_4;
        let obj_5;
        if with_overflow {
            obj_5 = add_object(s, b"55555", 5, false);
            obj_4 = add_object(s, &large_bytes, 50000, false);
        } else {
            obj_4 = add_object(s, &large_bytes, 50000, false);
            obj_5 = add_object(s, b"55555", 5, false);
        }

        start_object(s, &large_bytes, 20000);
        add_offset(s, obj_5);
        let obj_3 = s.pop_pack(false).unwrap();

        start_object(s, b"22", 2);
        add_virtual_offset(s, obj_5);
        let obj_2 = s.pop_pack(false).unwrap();

        // obj 1
        start_object(s, b"1", 1);
        add_offset(s, obj_2);
        add_offset(s, obj_3);
        add_offset(s, obj_4);

        s.pop_pack(false);
        s.end_serialize();
    }

    fn run_resolve_overflow_test(
        overflowing: &Serializer,
        expected: &Serializer,
        num_iterations: u8,
        recalculate_extensions: bool,
        check_binary_equivalence: bool,
        table_tag: Tag,
    ) {
        let mut graph = Graph::from_serializer(overflowing).unwrap();
        let mut expected_graph = Graph::from_serializer(expected).unwrap();

        if expected_graph.has_overflows() {
            if check_binary_equivalence {
                println!(
                "when binary equivalence checking is enabled, the expected graph cannot overflow."
            );
                assert!(!check_binary_equivalence);
            }
            expected_graph.assign_spaces().unwrap();
            expected_graph.sort_shortest_distance().unwrap();
        }
        // Check that overflow resolution succeeds
        assert!(graph.has_overflows());
        resolve_graph_overflows(
            &mut graph,
            table_tag,
            num_iterations,
            recalculate_extensions,
        )
        .unwrap();

        // Check the graphs can be serialized.
        let out1 = graph.serialize().unwrap();
        assert!(!out1.is_empty());
        let out2 = expected_graph.serialize().unwrap();
        assert!(!out2.is_empty());

        if check_binary_equivalence {
            assert_eq!(out1, out2);
        }

        // Check the graphs are equivalent
        graph.normalize();
        expected_graph.normalize();
        assert_eq!(graph, expected_graph);
    }

    #[test]
    fn test_resolve_overflows_via_sort() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_overflow(&mut s);
        let out = resolve_overflows(&s, Tag::from_u32(0), 32).unwrap();
        assert_eq!(out.len(), 80000 + 3 + 3 * 2);
    }

    #[test]
    fn test_resolve_overflows_via_duplication() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_dedup_overflow(&mut s);
        let out = resolve_overflows(&s, Tag::from_u32(0), 32).unwrap();
        assert_eq!(out.len(), 10000 + 2 * 2 + 60000 + 2 + 3 * 2);
    }

    #[test]
    fn test_resolve_overflows_via_multiple_duplication() {
        let buf_size = 300000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_multiple_dedup_overflow(&mut s);
        let out = resolve_overflows(&s, Tag::from_u32(0), 5).unwrap();
        assert!(!out.is_empty());
    }

    #[test]
    fn test_resolve_overflows_via_space_assignment() {
        let buf_size = 160000;
        let mut c = Serializer::new(buf_size);
        populate_serializer_spaces(&mut c, true);

        let mut e = Serializer::new(buf_size);
        populate_serializer_spaces(&mut e, false);

        run_resolve_overflow_test(&c, &e, 0, false, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_overflows_via_priority() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_priority_overflow(&mut s);

        let mut e = Serializer::new(buf_size);
        populate_serializer_with_priority_overflow_expected(&mut e);
        run_resolve_overflow_test(&s, &e, 3, false, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_overflows_via_isolation() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_isolation_overflow(&mut s);

        assert!(s.offset_overflow());
        let out = resolve_overflows(&s, Tag::new(b"GSUB"), 0).unwrap();
        assert!(!out.is_empty());
        assert_eq!(out.len(), 1 + 10000 + 60000 + 1 + 1 + 4 + 3 * 2);
    }

    #[test]
    fn test_resolve_overflows_via_isolation_with_recursive_duplication() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_isolation_overflow_complex(&mut s);

        let mut e = Serializer::new(buf_size);
        populate_serializer_with_isolation_overflow_complex_expected(&mut e);

        run_resolve_overflow_test(&s, &e, 0, false, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_overflows_via_isolation_spaces() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_isolation_overflow_spaces(&mut s);

        assert!(s.offset_overflow());
        let out = resolve_overflows(&s, Tag::new(b"GSUB"), 0).unwrap();
        assert!(!out.is_empty());

        // objects: 3 + 2 * 60000
        // links: 2 * 4 + 2 *  2
        let expected_length = 3 + 2 * 60000 + 2 * 4 + 2 * 2;
        assert_eq!(out.len(), expected_length);
    }

    #[test]
    fn test_resolve_overflows_via_isolating_16bit_space() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_spaces_16bit_connection(&mut s);

        let mut e = Serializer::new(buf_size);
        populate_serializer_spaces_16bit_connection_expected(&mut e);

        run_resolve_overflow_test(&s, &e, 0, false, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_overflows_via_isolating_16bit_space_2() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_short_and_wide_subgraph_root(&mut s);

        let mut e = Serializer::new(buf_size);
        populate_serializer_short_and_wide_subgraph_root_expected(&mut e);

        run_resolve_overflow_test(&s, &e, 0, false, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_overflows_via_splitting_spaces() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_split_spaces(&mut s);

        let mut e = Serializer::new(buf_size);
        populate_serializer_with_split_spaces_expected(&mut e);

        run_resolve_overflow_test(&s, &e, 1, false, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_overflows_via_splitting_spaces_2() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_split_spaces_2(&mut s);

        let mut e = Serializer::new(buf_size);
        populate_serializer_with_split_spaces_expected_2(&mut e);

        run_resolve_overflow_test(&s, &e, 1, false, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_mixed_overflows_via_isolation_spaces() {
        let buf_size = 200000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_24_and_32_bit_offsets(&mut s);

        assert!(s.offset_overflow());
        let out = resolve_overflows(&s, Tag::new(b"GSUB"), 0).unwrap();
        assert!(!out.is_empty());

        // objects: 7 + 4 * 40000
        // links:
        // 32bit: 2 * 4
        // 24bit: 4 * 3
        // 16bit: 4 * 2
        let expected_length = 7 + 4 * 40000 + 2 * 4 + 4 * 3 + 4 * 2;
        assert_eq!(out.len(), expected_length);
    }

    #[test]
    fn test_virtual_link() {
        let buf_size = 100;
        let mut c = Serializer::new(buf_size);
        populate_serializer_virtual_link(&mut c);

        let out = resolve_overflows(&c, Tag::from_u32(0), 32).unwrap();
        assert!(!out.is_empty());
        assert_eq!(out.len(), 5 + 4 * 2);
        assert_eq!(out[0], b'a');
        assert_eq!(out[5], b'c');
        assert_eq!(out[8], b'e');
        assert_eq!(out[9], b'b');
        assert_eq!(out[12], b'd');
    }

    #[test]
    fn test_repack_last() {
        let buf_size = 200000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_repack_last(&mut overflowing, true);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_repack_last(&mut expected, false);
        run_resolve_overflow_test(&overflowing, &expected, 20, false, true, Tag::new(b"abcd"));
    }

    #[test]
    fn test_dont_duplicate_virtual() {
        let buf_size = 200000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_virtual(&mut overflowing, true);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_virtual(&mut expected, false);
        run_resolve_overflow_test(&overflowing, &expected, 20, false, true, Tag::new(b"abcd"));
    }

    #[test]
    fn test_resolve_with_extension_promotion() {
        let buf_size = 200000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_extension_promotion(&mut overflowing, 0, false);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_extension_promotion(&mut expected, 3, false);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_with_shared_extension_promotion() {
        let buf_size = 200000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_extension_promotion(&mut overflowing, 0, true);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_extension_promotion(&mut expected, 3, true);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_with_basic_pair_pos_1_split() {
        let buf_size = 200000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_large_pair_pos_1(&mut overflowing, 1, 4, false);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_large_pair_pos_1(&mut expected, 2, 2, true);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gpos::TAG);
    }

    #[test]
    fn test_resolve_with_extension_pair_pos_1_split() {
        let buf_size = 200000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_large_pair_pos_1(&mut overflowing, 1, 4, true);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_large_pair_pos_1(&mut expected, 2, 2, true);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gpos::TAG);
    }

    #[test]
    fn test_resolve_with_basic_pair_pos_2_split() {
        let buf_size = 300000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_large_pair_pos_2(&mut overflowing, 1, 4, 3000, false, false, true);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_large_pair_pos_2(&mut expected, 2, 2, 3000, true, false, true);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gpos::TAG);
    }
    #[test]
    fn test_resolve_with_pair_pos_2_split_with_device_tables() {
        let buf_size = 300000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_large_pair_pos_2(&mut overflowing, 1, 4, 2000, false, true, true);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_large_pair_pos_2(&mut expected, 2, 2, 2000, true, true, true);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gpos::TAG);
    }

    #[test]
    fn test_resolve_with_close_to_limit_pair_pos_2_split() {
        let buf_size = 300000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_large_pair_pos_2(
            &mut overflowing,
            1,
            1636,
            10,
            true,
            false,
            false,
        );

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_large_pair_pos_2(&mut expected, 2, 818, 10, true, false, false);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gpos::TAG);
    }

    #[test]
    fn test_resolve_with_basic_mark_base_pos_1_split() {
        let buf_size = 200000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_large_mark_base_pos_1(&mut overflowing, 40, 10, 110, 1);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_large_mark_base_pos_1(&mut expected, 40, 10, 110, 2);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gpos::TAG);
    }

    #[test]
    fn test_resolve_with_basic_liga_split() {
        let buf_size = 400000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_large_liga(&mut overflowing, 1, 1, 2, 40000, false);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_large_liga(&mut expected, 2, 1, 1, 40000, false);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_with_liga_split_move() {
        let buf_size = 400000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_large_liga(&mut overflowing, 1, 6, 2, 16000, true);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_large_liga(&mut expected, 3, 2, 2, 16000, true);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_with_liga_split_overlapping_clone() {
        let buf_size = 400000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_large_liga(&mut overflowing, 1, 2, 3, 30000, true);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_large_liga_overlapping_clone_result(&mut expected);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_with_liga_split_shared_table() {
        let buf_size = 400000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_shared_large_liga(&mut overflowing, 1, 6, 2, 16000);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_shared_large_liga(&mut expected, 3, 2, 2, 16000);

        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gsub::TAG);
    }

    #[test]
    fn test_resolve_with_liga_split_shared_coverage() {
        let buf_size = 400000;
        let mut overflowing = Serializer::new(buf_size);
        populate_serializer_with_liga_shared_coverage(&mut overflowing, 1, 6, 2, 16000);

        let mut expected = Serializer::new(buf_size);
        populate_serializer_with_liga_shared_coverage(&mut expected, 3, 2, 2, 16000);
        run_resolve_overflow_test(&overflowing, &expected, 20, true, false, Gsub::TAG);
    }
}
