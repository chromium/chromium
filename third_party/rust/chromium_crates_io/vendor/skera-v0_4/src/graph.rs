//! Define a graph struct that represents a serialized table
//! Implement methods to modify and reorder the graph

use crate::fnv::FnvHashMap;
use crate::{
    priority_queue::PriorityQueue,
    serialize::{Link, LinkWidth, ObjIdx, Object, OffsetWhence, SerializeErrorFlags, Serializer},
};
use std::collections::BTreeMap;
use write_fonts::{read::collections::IntSet, types::Uint24};

mod coverage_graph;
pub(crate) mod layout;
pub(crate) mod ligature_graph;
pub(crate) mod markbasepos_graph;
pub(crate) mod pairpos_graph;

const MAX_SPACES: usize = 8000;
const MAX_VERTICES: usize = 100000;

#[derive(Debug)]
pub(crate) enum RepackError {
    GraphErrorOrphanedNodes,
    GraphErrorInvalidObjIndex,
    GraphErrorInvalidLinkPosition,
    GraphErrorCycleDetected,
    GraphErrorInvalidRoot,
    GraphErrorInvalidVertex,
    ErrorRepackSerialize,
    ErrorReadTable,
    ErrorSplitSubtable,
    ErrorNoResolution,
    ErrorMaxOperationsExceeded,
}

pub(crate) struct Overflow(u64);

impl Overflow {
    fn child(&self) -> ObjIdx {
        (self.0 as usize) & 0xFFFFFFFF
    }

    fn parent(&self) -> ObjIdx {
        (self.0 >> 32) as usize
    }
}

#[derive(Default, Debug)]
pub(crate) struct Vertex {
    head: usize,
    tail: usize,
    // real_links: link position-> Link mapping
    // real links are associated with actual offsets
    real_links: BTreeMap<u32, Link>,
    // virtual links not associated with actual offsets,
    // they exist merely to enforce an ordering constraint.
    virtual_links: Vec<Link>,

    distance: u64,
    space: usize,
    priority: u8,
    start: usize,
    end: usize,
    incoming_edges: usize,
    has_incoming_virtual_edges: bool,
    parents: FnvHashMap<ObjIdx, u16>,
}

impl Vertex {
    fn from_object(obj: &Object) -> Self {
        Self {
            head: obj.head(),
            tail: obj.tail(),
            real_links: obj.real_links(),
            virtual_links: obj.virtual_links(),
            ..Default::default()
        }
    }

    // create a copy of a vertex, children links are duplicated but parents are not
    fn duplicate(other_v: &Self) -> Self {
        Self {
            head: other_v.head,
            tail: other_v.tail,
            real_links: other_v.real_links.clone(),
            virtual_links: other_v.virtual_links.clone(),
            distance: other_v.distance,
            space: other_v.space,
            ..Default::default()
        }
    }

    pub(crate) fn table_size(&self) -> usize {
        self.tail - self.head
    }

    fn reset_parents(&mut self) {
        self.incoming_edges = 0;
        self.has_incoming_virtual_edges = false;
        self.parents.clear();
    }

    fn add_parent(&mut self, parent_idx: ObjIdx, is_virtual: bool) {
        self.has_incoming_virtual_edges |= is_virtual;
        self.parents.entry(parent_idx).and_modify(|c| *c += 1).or_insert(1);

        self.incoming_edges += 1;
    }

    fn remove_parent(&mut self, parent_idx: ObjIdx, remove_all_edges: bool) {
        let Some(num_edges) = self.parents.get_mut(&parent_idx) else {
            return;
        };

        if remove_all_edges {
            self.incoming_edges -= *num_edges as usize;
        } else {
            self.incoming_edges -= 1;
        }

        if *num_edges > 1 && !remove_all_edges {
            *num_edges -= 1;
        } else {
            self.parents.remove(&parent_idx);
        }
    }

    fn remap_parent(&mut self, old_parent: ObjIdx, new_parent: ObjIdx) {
        let Some(v) = self.parents.get(&old_parent) else {
            return;
        };

        self.parents.insert(new_parent, *v);
        self.parents.remove(&old_parent);
    }

    fn link_positions_valid(&self, num_objs: usize) -> bool {
        let table_size = self.table_size();
        let mut assigned_bytes = IntSet::empty();
        for (pos, l) in &self.real_links {
            if l.obj_idx() >= num_objs {
                return false;
            }

            let width = l.link_width() as u8;
            if width < 2 {
                return false;
            }

            let start = *pos;
            let end = start + width as u32 - 1;
            if end as usize >= table_size {
                return false;
            }

            if assigned_bytes.intersects_range(start..=end) {
                return false;
            }
            assigned_bytes.insert_range(start..=end);
        }
        true
    }

    fn has_max_priority(&self) -> bool {
        self.priority >= 3
    }

    fn raise_priority(&mut self) -> bool {
        if self.has_max_priority() {
            return false;
        }

        self.priority += 1;
        true
    }

    fn modified_distance(&self, order: u32) -> i64 {
        let prev_dist = self.distance as i64;
        let table_size = (self.tail - self.head) as i64;

        let distance = if self.has_max_priority() {
            0
        } else {
            match self.priority {
                0 => prev_dist,
                1 => prev_dist - table_size / 2,
                2 => prev_dist - table_size,
                _ => 0,
            }
            .clamp(0, 0x7FFFFFFFFFF_i64)
        };

        (distance << 18) | (0x003FFFF & order as i64)
    }

    fn incoming_edges(&self) -> usize {
        self.incoming_edges
    }

    fn is_shared(&self) -> bool {
        self.parents.len() > 1
    }

    fn is_leaf(&self) -> bool {
        self.real_links.is_empty() && self.virtual_links.is_empty()
    }

    fn incoming_edges_from_parent(&self, parent_idx: ObjIdx) -> u16 {
        *self.parents.get(&parent_idx).unwrap_or(&0)
    }

    fn give_max_priority(&mut self) {
        if !self.has_max_priority() {
            self.priority = 3;
        }
    }

    pub(crate) fn add_link(
        &mut self,
        width: LinkWidth,
        child_idx: ObjIdx,
        position: u32,
        is_virtual: bool,
    ) {
        let link = Link::new(width, child_idx, position);
        if is_virtual {
            self.virtual_links.push(link);
        } else {
            self.real_links.insert(position, link);
        }
    }

    pub(crate) fn child_idxes(&self) -> FnvHashMap<ObjIdx, u32> {
        let mut out = FnvHashMap::default();
        for (&pos, l) in &self.real_links {
            let obj_idx = l.obj_idx();
            out.entry(obj_idx)
                .and_modify(|p| {
                    if *p < pos {
                        *p = pos
                    }
                })
                .or_insert(pos);
        }
        out
    }

    pub(crate) fn remap_child(&mut self, pos: u32, new_child_idx: ObjIdx) {
        self.real_links.entry(pos).and_modify(|l| l.update_obj_idx(new_child_idx));
    }

    fn real_links(&self) -> &BTreeMap<u32, Link> {
        &self.real_links
    }
}

impl Clone for Vertex {
    fn clone(&self) -> Self {
        Self {
            head: self.head,
            tail: self.tail,
            real_links: self.real_links.clone(),
            virtual_links: self.virtual_links.clone(),
            distance: self.distance,
            space: self.space,
            ..Default::default()
        }
    }
}

//TODO: add support for space assignment and splitting
#[derive(Default, Debug)]
pub(crate) struct Graph {
    vertices: Vec<Vertex>,
    // an object's id will not change
    // the ordering vector stores sorted object ordering
    ordering: Vec<usize>,
    ordering_scratch: Vec<usize>,
    num_roots_for_space: Vec<usize>,
    data: Vec<u8>,

    // graph state flags
    parents_invalid: bool,
    distance_invalid: bool,
    positions_invalid: bool,
}

impl Graph {
    pub(crate) fn from_serializer(s: &Serializer) -> Result<Self, RepackError> {
        let packed_obj_idxs = s.packed_idxs();
        let count = packed_obj_idxs.len();
        let mut this = Graph {
            vertices: Vec::with_capacity(count),
            ordering: Vec::with_capacity(count),
            ordering_scratch: Vec::with_capacity(count),
            data: s.data(),
            parents_invalid: true,
            positions_invalid: true,
            distance_invalid: true,
            ..Default::default()
        };

        this.num_roots_for_space.push(1);
        for idx in packed_obj_idxs.iter() {
            let obj = s.get_obj(*idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

            let v = Vertex::from_object(obj);
            if !v.link_positions_valid(count) {
                return Err(RepackError::GraphErrorInvalidLinkPosition);
            }
            this.vertices.push(v);
        }

        let mut i = 0;
        this.ordering.resize_with(count, || {
            i += 1;
            count - i
        });
        Ok(this)
    }

    pub(crate) fn vertex(&self, obj_idx: ObjIdx) -> Option<&Vertex> {
        self.vertices.get(obj_idx)
    }

    fn mut_vertex(&mut self, obj_idx: ObjIdx) -> Option<&mut Vertex> {
        self.vertices.get_mut(obj_idx)
    }

    fn vertex_data(&self, obj_idx: ObjIdx) -> Option<&[u8]> {
        let v = self.vertex(obj_idx)?;
        self.data.get(v.head..v.tail)
    }

    fn vertex_data_mut(&mut self, obj_idx: ObjIdx) -> Option<&mut [u8]> {
        let v = self.vertices.get(obj_idx)?;
        self.data.get_mut(v.head..v.tail)
    }

    // Generates a new topological sorting of graph ordered by the shortest
    // distance to each node if positions are marked as invalid.
    pub(crate) fn sort_shortest_distance_if_needed(&mut self) -> Result<(), RepackError> {
        if !self.positions_invalid {
            return Ok(());
        }
        self.sort_shortest_distance()
    }

    // Generates a new topological sorting of graph ordered by the shortest
    // distance to each node.
    pub(crate) fn sort_shortest_distance(&mut self) -> Result<(), RepackError> {
        if self.vertices.len() < 2 {
            // no need to do sorting when num of nodes < 2
            return Ok(());
        }

        self.positions_invalid = true;
        self.update_distances()?;

        let v_count = self.vertices.len();
        let mut queue = PriorityQueue::with_capacity(v_count);
        self.ordering_scratch.resize(v_count, 0);
        let mut removed_edges = vec![0_usize; v_count];

        self.update_parents()?;

        queue.push((self.root().modified_distance(0), self.root_idx()));
        let mut order = 1_u32;
        let mut pos = 0;

        let new_ordering = &mut self.ordering_scratch;
        while let Some((_, next_id)) = queue.pop() {
            if pos >= v_count {
                // we're out of ids, meaning we've visited the same node more than once
                return Err(RepackError::GraphErrorCycleDetected);
            }
            new_ordering[pos] = next_id;
            pos += 1;

            let next_v = &self.vertices[next_id];
            for link in next_v.real_links.values().chain(next_v.virtual_links.iter()) {
                let child_idx = link.obj_idx();
                removed_edges[child_idx] += 1;

                let child_v = &self.vertices[child_idx];
                if child_v.incoming_edges() == removed_edges[child_idx] {
                    queue.push((child_v.modified_distance(order), child_idx));
                    order += 1;
                }
            }
        }

        std::mem::swap(&mut self.ordering, new_ordering);
        if pos != v_count {
            return Err(RepackError::GraphErrorOrphanedNodes);
            //TODO: add print_orphaned_nodes()
        }
        Ok(())
    }

    fn root(&self) -> &Vertex {
        &self.vertices[self.root_idx()]
    }

    pub(crate) fn update_parents(&mut self) -> Result<(), RepackError> {
        if !self.parents_invalid {
            return Ok(());
        }

        for v in self.vertices.iter_mut() {
            v.reset_parents();
        }

        let count = self.vertices.len();
        let mut real_links_idxes = Vec::with_capacity(count);
        let mut virtual_links_idxes = Vec::with_capacity(count);
        for idx in 0..count {
            let v = &self.vertices[idx];

            real_links_idxes.clear();
            virtual_links_idxes.clear();
            for l in v.real_links.values() {
                real_links_idxes.push(l.obj_idx());
            }

            for l in &v.virtual_links {
                virtual_links_idxes.push(l.obj_idx());
            }

            for child_idx in &real_links_idxes {
                let Some(v) = self.vertices.get_mut(*child_idx) else {
                    return Err(RepackError::GraphErrorInvalidObjIndex);
                };
                v.add_parent(idx, false);
            }

            for child_idx in &virtual_links_idxes {
                let Some(v) = self.vertices.get_mut(*child_idx) else {
                    return Err(RepackError::GraphErrorInvalidObjIndex);
                };
                v.add_parent(idx, true);
            }
        }
        Ok(())
    }

    // Finds the distance to each object in the graph from the root node
    // Uses Dijkstra's algorithm to find all of the shortest distances.
    // ref: <https://github.com/harfbuzz/harfbuzz/blob/3f70b6987830bba1e4922cad03028cdd9d78b3a1/src/graph/graph.hh#L1512C13-L1512C72>
    pub(crate) fn update_distances(&mut self) -> Result<(), RepackError> {
        if !self.distance_invalid {
            return Ok(());
        }

        for i in self.vertices.iter_mut() {
            i.distance = u64::MAX / 2;
        }
        let root_idx = self.root_idx();
        self.vertices[root_idx].distance = 0;

        let count = self.vertices.len();
        let mut queue = PriorityQueue::with_capacity(count);
        queue.push((0_u64, root_idx));

        let mut visited = vec![false; count];
        let mut distance_map = vec![0; count];
        while let Some((next_distance, next_idx)) = queue.pop() {
            if visited[next_idx] {
                continue;
            }

            let next_v = &self.vertices[next_idx];
            visited[next_idx] = true;

            for link in next_v.real_links.values().chain(next_v.virtual_links.iter()) {
                let child_idx = link.obj_idx();
                if visited[child_idx] {
                    continue;
                }

                let child_v = &self.vertices[child_idx];
                let link_width =
                    if link.link_width() == LinkWidth::Zero { 4 } else { link.link_width() as u8 };

                let child_weight =
                    child_v.tail - child_v.head + (1 << (link_width * 8)) * (child_v.space + 1);
                let child_distance = next_distance + child_weight as u64;
                if child_distance < child_v.distance {
                    distance_map[child_idx] = child_distance;
                    queue.push((child_distance, child_idx));
                }
            }
        }

        // to avoid multiple references issue, update distances from the map
        for (distance, v) in distance_map.iter().zip(self.vertices.iter_mut()) {
            v.distance = *distance;
        }

        if !queue.is_empty() {
            return Err(RepackError::GraphErrorOrphanedNodes);
        }

        self.distance_invalid = false;
        Ok(())
    }

    pub(crate) fn root_idx(&self) -> usize {
        self.ordering[0]
    }

    pub(crate) fn is_fully_connected(&mut self) -> Result<(), RepackError> {
        self.update_parents()?;

        // Root cannot have parents
        if self.root().incoming_edges() > 0 {
            return Err(RepackError::GraphErrorInvalidRoot);
        }

        let root_idx = self.root_idx();
        if self.vertices.iter().take(root_idx).any(|v| v.incoming_edges() == 0) {
            return Err(RepackError::GraphErrorOrphanedNodes);
        }
        Ok(())
    }

    fn total_size_in_bytes(&self) -> usize {
        self.vertices.iter().map(|v| v.tail - v.head).reduce(|acc, e| acc + e).unwrap_or(0)
    }

    pub(crate) fn serialize(&self) -> Result<Vec<u8>, SerializeErrorFlags> {
        let mut s = Serializer::new(self.total_size_in_bytes());
        s.start_serialize()?;

        let vertices = &self.vertices;
        let data = &self.data;
        // ref: <https://github.com/harfbuzz/harfbuzz/blob/07ee609f5abe59b591e4a6cf99db890be556501b/src/graph/serialize.hh#L245>
        let mut id_map = vec![0; self.ordering.len()];
        for i in self.ordering.iter().rev() {
            let v = &vertices[*i];

            let obj_bytes = &data[v.head..v.tail];
            let obj_size = obj_bytes.len();
            s.push()?;
            let start = s.embed_bytes(obj_bytes)?;

            for (link_pos, link) in &v.real_links {
                serialize_link(&mut s, link, *link_pos as usize, start, obj_size, &id_map)?;
            }

            let new_idx = s.pop_pack(false).ok_or(SerializeErrorFlags::SERIALIZE_ERROR_OTHER)?;
            id_map[*i] = new_idx;
        }
        s.end_serialize();

        if s.in_error() {
            return Err(s.error());
        }

        Ok(s.copy_bytes())
    }

    // compute the serialized start/end positions for each vertex
    fn update_positions(&mut self) {
        if !self.positions_invalid {
            return;
        }

        let mut cur_pos = 0;
        let vertices = &mut self.vertices;
        for i in &self.ordering {
            let v = &mut vertices[*i];
            v.start = cur_pos;
            cur_pos += v.tail - v.head;
            v.end = cur_pos;
        }

        self.positions_invalid = false;
    }

    #[inline]
    fn offset_overflows(&self, parent_v: &Vertex, link: &Link) -> bool {
        let vertices = &self.vertices;
        let child_v = &vertices[link.obj_idx()];
        let mut offset = match link.whence() {
            OffsetWhence::Head => child_v.start - parent_v.start,
            OffsetWhence::Tail => child_v.start - parent_v.end,
            OffsetWhence::Absolute => child_v.start,
        };

        let bias = link.bias() as usize;
        assert!(offset >= bias);
        offset -= bias;

        //TODO: support signed offset?
        match link.link_width() {
            LinkWidth::Two => offset > u16::MAX as usize,
            LinkWidth::Three => offset > (Uint24::MAX).to_u32() as usize,
            LinkWidth::Four => offset > u32::MAX as usize,
            LinkWidth::Zero => false,
        }
    }

    pub(crate) fn has_overflows(&mut self) -> bool {
        self.update_positions();
        let vertices = &self.vertices;
        for parent_idx in &self.ordering {
            let parent_v = &vertices[*parent_idx];
            for link in parent_v.real_links.values() {
                if self.offset_overflows(parent_v, link) {
                    return true;
                }
            }
        }
        false
    }

    pub(crate) fn overflows(&mut self) -> Vec<Overflow> {
        self.update_positions();
        let vertices = &self.vertices;
        let mut overflows = FnvHashMap::default();
        let mut out = Vec::new();
        for parent_idx in &self.ordering {
            let parent_v = &vertices[*parent_idx];
            for link in parent_v.real_links.values() {
                if !self.offset_overflows(parent_v, link) {
                    continue;
                }

                let overflow = ((*parent_idx as u64) << 32) | (link.obj_idx() as u64);
                if overflows.insert(overflow, 1).is_some() {
                    continue;
                }
                out.push(Overflow(overflow));
            }
        }
        out
    }

    pub(crate) fn assign_spaces(&mut self) -> Result<bool, RepackError> {
        self.update_parents()?;
        let (mut roots, mut visited) = self.find_space_roots()?;
        if roots.is_empty() {
            return Ok(false);
        }

        visited.invert();
        loop {
            let Some(next) = roots.first() else {
                break;
            };
            let mut connected_roots = IntSet::empty();
            self.find_connected_nodes(
                next as usize,
                &mut roots,
                &mut visited,
                &mut connected_roots,
            );

            self.isolate_subgraph(&mut connected_roots)?;
            let next_space = self.next_space();
            if next_space >= MAX_SPACES {
                return Err(RepackError::ErrorMaxOperationsExceeded);
            }

            self.num_roots_for_space.push(connected_roots.len() as usize);
            self.distance_invalid = true;
            self.positions_invalid = true;

            let vertices = &mut self.vertices;
            for obj_idx in connected_roots.iter() {
                vertices[obj_idx as usize].space = next_space;
            }
        }
        Ok(true)
    }

    // Finds all nodes in targets that are reachable from start_idx, nodes in
    // visited will be skipped. For this search the graph is treated as being
    // undirected. Connected targets will be added to connected and removed from
    // targets. All visited nodes will be added to visited.
    fn find_connected_nodes(
        &self,
        start_idx: ObjIdx,
        targets: &mut IntSet<u32>,
        visited: &mut IntSet<u32>,
        connected: &mut IntSet<u32>,
    ) {
        if !visited.insert(start_idx as u32) {
            return;
        }

        if targets.remove(start_idx as u32) {
            connected.insert(start_idx as u32);
        }

        let v = &self.vertices[start_idx];
        for l in v.real_links.values().chain(v.virtual_links.iter()) {
            self.find_connected_nodes(l.obj_idx(), targets, visited, connected);
        }

        for p in v.parents.keys() {
            self.find_connected_nodes(*p, targets, visited, connected);
        }
    }

    fn find_space_roots(&self) -> Result<(IntSet<u32>, IntSet<u32>), RepackError> {
        let root_idx = self.root_idx();
        let mut visited = IntSet::empty();
        let mut roots = IntSet::empty();
        let vertices = &self.vertices;
        for i in &self.ordering {
            if visited.contains(*i as u32) {
                continue;
            }
            let Some(v) = vertices.get(*i) else {
                return Err(RepackError::GraphErrorInvalidObjIndex);
            };
            for l in v.real_links.values() {
                if l.is_signed() {
                    continue;
                }

                match l.link_width() {
                    LinkWidth::Three => {
                        if *i == root_idx {
                            continue;
                        }
                        let mut sub_roots = IntSet::empty();
                        let obj_idx = l.obj_idx();
                        self.find_32bit_roots(obj_idx, &mut sub_roots);
                        if sub_roots.is_empty() {
                            roots.insert(obj_idx as u32);
                            self.find_subgraph_nodes(obj_idx, &mut visited);
                        } else {
                            for idx in sub_roots.iter() {
                                roots.insert(idx);
                                self.find_subgraph_nodes(idx as usize, &mut visited);
                            }
                        }
                    }
                    LinkWidth::Four => {
                        let obj_idx = l.obj_idx();
                        roots.insert(obj_idx as u32);
                        self.find_subgraph_nodes(obj_idx, &mut visited);
                    }
                    _ => continue,
                }
            }
        }
        Ok((roots, visited))
    }

    fn find_subgraph_nodes(&self, obj_idx: ObjIdx, subgraph: &mut IntSet<u32>) {
        if !subgraph.insert(obj_idx as u32) {
            return;
        }

        let v = &self.vertices[obj_idx];
        for l in v.real_links.values().chain(v.virtual_links.iter()) {
            self.find_subgraph_nodes(l.obj_idx(), subgraph);
        }
    }

    fn find_subgraph_nodes_incoming_edges(
        &self,
        start_idx: ObjIdx,
        subgraph_map: &mut FnvHashMap<u32, usize>,
    ) {
        let v = &self.vertices[start_idx];
        for l in v.real_links.values().chain(v.virtual_links.iter()) {
            let obj_idx = l.obj_idx();
            let v = subgraph_map.entry(obj_idx as u32).and_modify(|c| *c += 1).or_insert(1);

            if *v > 1 {
                continue;
            }
            self.find_subgraph_nodes_incoming_edges(obj_idx, subgraph_map);
        }
    }

    pub(crate) fn find_subgraph_size(
        &self,
        obj_idx: ObjIdx,
        visited: &mut IntSet<u32>,
        max_depth: u16,
    ) -> Result<usize, RepackError> {
        if !visited.insert(obj_idx as u32) {
            return Ok(0);
        }

        assert!(obj_idx < self.vertices.len());
        let v = self.vertex(obj_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
        let mut size = v.table_size();
        if max_depth == 0 {
            return Ok(size);
        }

        for l in v.real_links.values().chain(v.virtual_links.iter()) {
            size += self.find_subgraph_size(l.obj_idx(), visited, max_depth - 1)?;
        }

        Ok(size)
    }

    // Finds the topmost children of 32bit offsets in the subgraph starting at
    // obj_idx
    fn find_32bit_roots(&self, obj_idx: ObjIdx, roots: &mut IntSet<u32>) {
        let v = &self.vertices[obj_idx];
        for l in v.real_links.values() {
            let child_idx = l.obj_idx();
            if !l.is_signed() && l.link_width() == LinkWidth::Four {
                roots.insert(child_idx as u32);
                continue;
            }
            self.find_32bit_roots(child_idx, roots);
        }
    }

    // Isolates the subgraph of nodes reachable from root. Any links to nodes in the
    // subgraph that originate from outside of the subgraph will be removed by
    // duplicating the linked to object
    // Indices stored in roots will be updated if any of the roots are duplicated to
    // new indices.
    fn isolate_subgraph(&mut self, roots: &mut IntSet<u32>) -> Result<(), RepackError> {
        self.update_parents()?;

        let mut parents = IntSet::empty();
        let mut subgraph_map = FnvHashMap::default();
        for root_idx in roots.iter() {
            subgraph_map.insert(root_idx, self.wide_parents(root_idx as usize, &mut parents));
            self.find_subgraph_nodes_incoming_edges(root_idx as usize, &mut subgraph_map);
        }

        let len = self.vertices.len();
        let mut index_map = FnvHashMap::default();
        for (idx, num_incoming_edges) in subgraph_map.iter() {
            let obj_idx = *idx as usize;
            assert!(obj_idx < len);
            // duplicate objects with incoming links from outside the subgraph.
            if *num_incoming_edges < self.vertices[obj_idx].incoming_edges() {
                self.duplicate_subgraph(obj_idx, &mut index_map)?;
            }
        }

        if index_map.is_empty() {
            return Ok(());
        }

        let new_subgraph = subgraph_map
            .keys()
            .map(|idx| index_map.get(&(*idx as usize)).copied().unwrap_or(*idx as usize))
            .map(|i| i as u32)
            .collect();

        self.remap_obj_indices(&index_map, new_subgraph, false)?;
        self.remap_obj_indices(&index_map, parents, true)?;

        for (old, new) in index_map.iter() {
            if roots.remove(*old as u32) {
                roots.insert(*new as u32);
            }
        }
        Ok(())
    }

    fn remap_obj_indices(
        &mut self,
        index_map: &FnvHashMap<usize, usize>,
        it: IntSet<u32>,
        only_wide: bool,
    ) -> Result<(), RepackError> {
        let mut old_to_new_idx_parents = Vec::new();
        for i in it.iter() {
            let parent_idx = i as usize;
            let Some(obj) = self.vertices.get_mut(i as usize) else {
                return Err(RepackError::GraphErrorInvalidObjIndex);
            };
            for l in obj.real_links.values_mut() {
                let old_idx = l.obj_idx();
                let Some(new_idx) = index_map.get(&old_idx) else {
                    continue;
                };

                if only_wide
                    && (l.is_signed()
                        || (l.link_width() != LinkWidth::Four
                            && l.link_width() != LinkWidth::Three))
                {
                    continue;
                }
                l.update_obj_idx(*new_idx);
                old_to_new_idx_parents.push((old_idx, *new_idx, parent_idx, false));
            }

            for l in &mut obj.virtual_links {
                let old_idx = l.obj_idx();
                let Some(new_idx) = index_map.get(&old_idx) else {
                    continue;
                };

                if only_wide
                    && (l.is_signed()
                        || (l.link_width() != LinkWidth::Four
                            && l.link_width() != LinkWidth::Three))
                {
                    continue;
                }
                l.update_obj_idx(*new_idx);
                old_to_new_idx_parents.push((old_idx, *new_idx, parent_idx, true));
            }
        }
        self.reassign_parents(&old_to_new_idx_parents)
    }

    // Also Corrects the parents map on the previous and new child nodes.
    fn reassign_parents(
        &mut self,
        old_to_new_idx_parents: &[(usize, usize, usize, bool)],
    ) -> Result<(), RepackError> {
        let vertices = &mut self.vertices;
        for (old_idx, new_idx, parent_idx, is_virtual) in old_to_new_idx_parents {
            let Some(old_v) = vertices.get_mut(*old_idx) else {
                return Err(RepackError::GraphErrorInvalidObjIndex);
            };
            old_v.remove_parent(*parent_idx, false);

            let Some(new_v) = vertices.get_mut(*new_idx) else {
                return Err(RepackError::GraphErrorInvalidObjIndex);
            };
            new_v.add_parent(*parent_idx, *is_virtual);
        }
        Ok(())
    }

    // duplicates all nodes in the subgraph reachable from start_idx. Does not
    // re-assign links. index_map is updated with mappings from old idx to new
    // idx. If a duplication has already been performed for a given index, then
    // it will be skipped.
    fn duplicate_subgraph(
        &mut self,
        start_idx: ObjIdx,
        index_map: &mut FnvHashMap<usize, usize>,
    ) -> Result<(), RepackError> {
        if index_map.contains_key(&start_idx) {
            return Ok(());
        }

        let clone_idx = self.duplicate_vertex(start_idx)?;
        index_map.insert(start_idx, clone_idx);

        let start_v = &self.vertices[start_idx];
        let child_idxes: Vec<ObjIdx> = start_v
            .real_links
            .values()
            .chain(start_v.virtual_links.iter())
            .map(|l| l.obj_idx())
            .collect();
        for idx in child_idxes {
            self.duplicate_subgraph(idx, index_map)?;
        }
        Ok(())
    }

    /// Creates a copy of the specified vertex and returns the new vertex idx.
    fn duplicate_vertex(&mut self, obj_idx: ObjIdx) -> Result<ObjIdx, RepackError> {
        let clone_idx = self.vertices.len();
        if clone_idx >= MAX_VERTICES {
            return Err(RepackError::ErrorMaxOperationsExceeded);
        }
        self.positions_invalid = true;
        self.distance_invalid = true;

        self.ordering.push(clone_idx);

        let v = self.vertex(obj_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;
        let new_v = Vertex::duplicate(v);
        let vertices = &mut self.vertices;

        for l in new_v.real_links.values() {
            vertices
                .get_mut(l.obj_idx())
                .ok_or(RepackError::GraphErrorInvalidObjIndex)?
                .add_parent(clone_idx, false);
        }

        for l in &new_v.virtual_links {
            vertices
                .get_mut(l.obj_idx())
                .ok_or(RepackError::GraphErrorInvalidObjIndex)?
                .add_parent(clone_idx, true);
        }

        vertices.push(new_v);
        Ok(clone_idx)
    }

    // returns the number of incoming edges that are 24 or 32 bits wide
    // and parent obj_idx will be added into the parents set
    fn wide_parents(&self, obj_idx: ObjIdx, parents_set: &mut IntSet<u32>) -> usize {
        let vertices = &self.vertices;
        let v = &vertices[obj_idx];
        let mut count = 0;
        for p in v.parents.keys() {
            let parent_v = &vertices[*p];
            for l in parent_v.real_links.values() {
                let width = l.link_width() as u8;
                if l.obj_idx() == obj_idx && (width == 3 || width == 4) && !l.is_signed() {
                    count += 1;
                    parents_set.insert(*p as u32);
                }
            }
        }
        count
    }

    fn next_space(&self) -> usize {
        self.num_roots_for_space.len()
    }

    pub(crate) fn try_isolating_subgraphs(
        &mut self,
        overflows: &[Overflow],
    ) -> Result<bool, RepackError> {
        let mut space = 0;
        let mut roots_to_isolate = IntSet::empty();
        for overflow in overflows.iter().rev() {
            let (overflow_space, root) = self.find_root_and_space(overflow.parent())?;
            if overflow_space == 0 || self.num_roots_for_space[overflow_space] < 2 {
                continue;
            }

            if space == 0 {
                space = overflow_space;
            }

            if space == overflow_space {
                roots_to_isolate.insert(root as u32);
            }
        }

        if roots_to_isolate.is_empty() {
            return Ok(false);
        }

        let max_to_move = self.num_roots_for_space[space] / 2;
        if roots_to_isolate.len() as usize > max_to_move {
            let mut extra = roots_to_isolate.len() as usize - max_to_move;
            for idx in &self.ordering {
                if extra == 0 {
                    break;
                }
                if roots_to_isolate.remove(*idx as u32) {
                    extra -= 1;
                }
            }
        }

        self.isolate_subgraph(&mut roots_to_isolate)?;
        self.move_to_new_space(&roots_to_isolate, space)?;
        Ok(true)
    }

    fn find_root_and_space(&self, obj_idx: ObjIdx) -> Result<(usize, ObjIdx), RepackError> {
        let Some(v) = self.vertices.get(obj_idx) else {
            return Err(RepackError::GraphErrorInvalidObjIndex);
        };
        if v.space > 0 {
            return Ok((v.space, obj_idx));
        }

        let Some(parent_idx) = v.parents.keys().nth(0) else {
            return Ok((0, obj_idx));
        };
        self.find_root_and_space(*parent_idx)
    }

    fn move_to_new_space(
        &mut self,
        indices: &IntSet<u32>,
        old_space: usize,
    ) -> Result<(), RepackError> {
        let new_space = self.num_roots_for_space.len();
        if new_space >= MAX_SPACES {
            return Err(RepackError::ErrorMaxOperationsExceeded);
        }
        let vertices = &mut self.vertices;
        for idx in indices.iter() {
            vertices[idx as usize].space = new_space;
        }

        let num_indices = indices.len() as usize;
        self.num_roots_for_space[old_space] -= num_indices;
        self.num_roots_for_space.push(num_indices);
        self.distance_invalid = true;
        self.positions_invalid = true;
        Ok(())
    }

    fn raise_childrens_priority(&mut self, parent_idx: ObjIdx) -> bool {
        let children: Vec<usize> = self.vertices[parent_idx]
            .real_links
            .values()
            .chain(self.vertices[parent_idx].virtual_links.iter())
            .map(|l| l.obj_idx())
            .collect();

        let mut made_changes = false;
        for obj_idx in children {
            made_changes |= self.vertices[obj_idx].raise_priority();
        }
        made_changes
    }

    // Creates a copy of child and re-assigns the link from parents to the clone.
    // The copy is a shallow copy, objects linked from child are not duplicated.
    // Returns the index of the newly created duplicate.
    // If the child_idx only has incoming edges from parents, duplication isn't
    // possible and this will return None
    fn duplicate_and_reassign_parents(
        &mut self,
        parents: &IntSet<u32>,
        child_idx: ObjIdx,
    ) -> Result<Option<ObjIdx>, RepackError> {
        if parents.is_empty() {
            return Ok(None);
        }

        self.update_parents()?;
        let child_v = &self.vertices[child_idx];
        if child_v.has_incoming_virtual_edges {
            return Ok(None);
        }

        let links_to_child = parents
            .iter()
            .map(|idx| child_v.incoming_edges_from_parent(idx as usize) as usize)
            .reduce(|acc, e| acc + e)
            .unwrap_or(0);
        if links_to_child >= child_v.incoming_edges() {
            return Ok(None);
        }

        let clone_idx = self.duplicate_vertex(child_idx)?;
        let mut old_to_new_idx_parents = Vec::new();
        for parent_idx in parents.iter() {
            let parent_idx = parent_idx as usize;
            for l in self.vertices[parent_idx].real_links.values_mut() {
                if l.obj_idx() != child_idx {
                    continue;
                }
                l.update_obj_idx(clone_idx);
                old_to_new_idx_parents.push((child_idx, clone_idx, parent_idx, false));
            }

            for l in self.vertices[parent_idx].virtual_links.iter_mut() {
                if l.obj_idx() != child_idx {
                    continue;
                }
                l.update_obj_idx(clone_idx);
                old_to_new_idx_parents.push((child_idx, clone_idx, parent_idx, true));
            }
        }
        self.reassign_parents(&old_to_new_idx_parents)?;
        Ok(Some(clone_idx))
    }

    // Creates a copy of child and re-assigns the link at specified position from
    // parent to the clone. The copy is a shallow copy, objects linked from
    // child are not duplicated. Returns the index of the newly created
    // duplicate.
    fn duplicate_child_at_position(
        &mut self,
        parent_idx: ObjIdx,
        child_idx: ObjIdx,
        pos: u32,
    ) -> Result<Option<ObjIdx>, RepackError> {
        let clone_idx = self.duplicate_vertex(child_idx)?;
        self.mut_vertex(child_idx)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?
            .remove_parent(parent_idx, false);

        self.mut_vertex(clone_idx)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?
            .add_parent(parent_idx, false);

        let Some(l) = self
            .mut_vertex(parent_idx)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?
            .real_links
            .get_mut(&pos)
        else {
            return Ok(None);
        };
        l.update_obj_idx(clone_idx);
        Ok(Some(clone_idx))
    }

    fn resolve_shared_overflow(
        &mut self,
        overflow: &Overflow,
        overflows: &[Overflow],
    ) -> Result<bool, RepackError> {
        // Find all of the parents in overflowing links that link to this same child
        // node. try duplicating the child node and re-assigning all of these
        // parents to the duplicate.
        let child_idx = overflow.child();
        let mut parents = IntSet::empty();
        for o in overflows {
            if o.child() == child_idx {
                parents.insert(o.parent() as u32);
            }
        }

        let Some(ret) = (match self.duplicate_and_reassign_parents(&parents, child_idx)? {
            None => {
                if parents.len() > 2 {
                    parents.remove(parents.first().unwrap());
                    self.duplicate_and_reassign_parents(&parents, child_idx)?
                } else {
                    None
                }
            }
            Some(clone_idx) => Some(clone_idx),
        }) else {
            return Ok(false);
        };

        if parents.len() > 1 {
            self.vertices[ret].give_max_priority();
        }
        Ok(true)
    }

    // ref:<https://github.com/harfbuzz/harfbuzz/blob/8f2ecfb10303d9970c79f27fc8af0a8f686302f6/src/hb-repacker.hh#L296>
    pub(crate) fn process_overflows(
        &mut self,
        overflows: &[Overflow],
    ) -> Result<bool, RepackError> {
        let mut priority_bumped_parents = IntSet::empty();
        let mut resolution_attempted = false;
        // try resolving the furthest overflows first
        for overflow in overflows.iter().rev() {
            let child_idx = overflow.child();
            if self.vertices[child_idx].is_shared()
                && self.resolve_shared_overflow(overflow, overflows)?
            {
                return Ok(true);
            }

            let parent_idx = overflow.parent();
            if self.vertices[child_idx].is_leaf()
                && !priority_bumped_parents.contains(parent_idx as u32)
                && self.raise_childrens_priority(parent_idx)
            {
                priority_bumped_parents.insert(parent_idx as u32);
                resolution_attempted = true;
            }
        }
        Ok(resolution_attempted)
    }

    //  Adds a new vertex to the graph, not connected to anything.
    fn new_vertex(&mut self, size: usize) -> Result<ObjIdx, RepackError> {
        let new_idx = self.vertices.len();
        if new_idx >= MAX_VERTICES {
            return Err(RepackError::ErrorMaxOperationsExceeded);
        }

        self.positions_invalid = true;
        self.distance_invalid = true;

        let cur_len = self.data.len();
        if size > 0 {
            self.data.resize(cur_len + size, 0);
        }

        let new_vertex = Vertex {
            head: cur_len,
            tail: cur_len + size,
            distance: 0,
            space: 0,
            ..Default::default()
        };

        self.vertices.push(new_vertex);
        self.ordering.push(new_idx);

        Ok(new_idx)
    }

    // Finds the object idx of the object pointed to by the offset at specified
    // 'position' within vertices[idx].
    pub(crate) fn index_for_position(&self, idx: ObjIdx, position: u32) -> Option<ObjIdx> {
        let v = self.vertices.get(idx)?;
        let link = v.real_links.get(&position)?;
        Some(link.obj_idx())
    }

    fn add_parent_child_link(
        &mut self,
        parent_idx: ObjIdx,
        child_idx: ObjIdx,
        width: LinkWidth,
        position: u32,
        is_virtual: bool,
    ) -> Result<(), RepackError> {
        self.vertices
            .get_mut(parent_idx)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?
            .add_link(width, child_idx, position, is_virtual);

        self.vertices
            .get_mut(child_idx)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?
            .add_parent(parent_idx, is_virtual);
        Ok(())
    }

    // Moves the child of old_parent_idx pointed to by old_offset to a new vertex at
    // the new_offset. Returns the idx of the child that was moved
    fn move_child(
        &mut self,
        old_parent_idx: ObjIdx,
        old_offset: u32,
        new_parent_idx: ObjIdx,
        new_offset: u32,
        link_width: usize,
    ) -> Result<Option<ObjIdx>, RepackError> {
        self.distance_invalid = true;
        self.positions_invalid = true;

        let old_parent_v = self.vertices.get_mut(old_parent_idx).unwrap();

        // remove from old parent
        let Some((_, link)) = old_parent_v.real_links.remove_entry(&old_offset) else {
            return Ok(None);
        };

        let child_idx = link.obj_idx();
        let width = LinkWidth::new_checked(link_width).ok_or(RepackError::ErrorSplitSubtable)?;

        self.vertices
            .get_mut(new_parent_idx)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?
            .add_link(width, child_idx, new_offset, false);

        let child_v =
            self.vertices.get_mut(child_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

        child_v.remove_parent(old_parent_idx, false);
        child_v.add_parent(new_parent_idx, false);
        Ok(Some(child_idx))
    }

    // Move all outgoing links in old parent that have a link position between
    // [old_offset_start, old_offset_start + num_child * link_width) to the new
    // parent. Links are placed serially in the new parent starting at
    // new_offset_start.
    fn move_children(
        &mut self,
        old_parent_idx: ObjIdx,
        old_offset_start: u32,
        new_parent_idx: ObjIdx,
        new_offset_start: u32,
        num_child: u32,
        link_width: usize,
    ) -> Result<(), RepackError> {
        self.distance_invalid = true;
        self.positions_invalid = true;

        let mut child_idxes = Vec::with_capacity(num_child as usize);

        let old_parent_v =
            self.vertices.get_mut(old_parent_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

        for i in 0..num_child {
            let pos = old_offset_start + i * link_width as u32;
            let Some((_, l)) = old_parent_v.real_links.remove_entry(&pos) else {
                continue;
            };

            child_idxes.push(l.obj_idx());
        }

        for child_idx in &child_idxes {
            let child_v =
                self.vertices.get_mut(*child_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

            child_v.remove_parent(old_parent_idx, false);
            child_v.add_parent(new_parent_idx, false);
        }

        let new_parent_v =
            self.vertices.get_mut(new_parent_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

        let width = LinkWidth::new_checked(link_width).ok_or(RepackError::ErrorSplitSubtable)?;
        for (i, child_idx) in child_idxes.iter().enumerate() {
            new_parent_v.add_link(
                width,
                *child_idx,
                new_offset_start + (i * link_width) as u32,
                false,
            );
        }
        Ok(())
    }

    fn update_vertex_data(&mut self, vertex_idx: ObjIdx, data: &[u8]) -> Result<(), RepackError> {
        let v = self.vertex(vertex_idx).ok_or(RepackError::GraphErrorInvalidObjIndex)?;

        let data_len = data.len();
        let table_size = v.table_size();
        if data_len <= table_size {
            let v = self.mut_vertex(vertex_idx).ok_or(RepackError::GraphErrorInvalidVertex)?;
            v.tail = v.head + data_len;

            self.vertex_data_mut(vertex_idx)
                .ok_or(RepackError::GraphErrorInvalidVertex)?
                .copy_from_slice(data);
        } else {
            let head = self.data.len();
            self.data.extend_from_slice(data);
            let v = self.mut_vertex(vertex_idx).ok_or(RepackError::GraphErrorInvalidVertex)?;

            v.head = head;
            v.tail = head + data_len;
        }
        Ok(())
    }

    fn remap_child(
        &mut self,
        parent: ObjIdx,
        old_child: ObjIdx,
        new_child: ObjIdx,
        pos: u32,
        is_virtual: bool,
    ) -> Result<(), RepackError> {
        self.mut_vertex(old_child)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?
            .remove_parent(parent, false);
        self.mut_vertex(new_child)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?
            .add_parent(parent, is_virtual);

        self.mut_vertex(parent)
            .ok_or(RepackError::GraphErrorInvalidObjIndex)?
            .remap_child(pos, new_child);
        Ok(())
    }
}

fn serialize_link(
    s: &mut Serializer,
    link: &Link,
    link_pos: usize,
    start: usize,
    obj_size: usize,
    id_map: &[usize],
) -> Result<(), SerializeErrorFlags> {
    let link_width = link.link_width() as usize;
    if link_width == 0 {
        return Ok(());
    }
    assert!(link_pos + link_width <= obj_size);

    let offset_pos = start + link_pos;
    let Some(offset_data) = s.get_mut_data(offset_pos..offset_pos + link_width) else {
        return Err(s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER));
    };

    offset_data.fill(0);
    let new_obj_idx = id_map
        .get(link.obj_idx())
        .ok_or_else(|| s.set_err(SerializeErrorFlags::SERIALIZE_ERROR_OTHER))?;

    s.add_link(
        offset_pos..offset_pos + link_width,
        *new_obj_idx,
        link.whence(),
        link.bias(),
        link.is_signed(),
    )
}

#[cfg(test)]
pub(crate) mod test {
    use super::*;
    use crate::serialize::OffsetWhence;
    use write_fonts::types::{FixedSize, Offset16, Offset24, Offset32, Scalar};

    impl Vertex {
        // zeros all offsets
        fn normalize(&self, data_bytes: &mut [u8]) {
            let head = self.head;
            for (pos, l) in self.real_links.iter() {
                let pos = head + *pos as usize;
                let width = l.link_width() as u8;
                data_bytes.get_mut(pos..pos + width as usize).unwrap().fill(0);
            }
        }

        //TODO: add debugging messages
        fn equals(&self, other: &Self, this_graph: &Graph, other_graph: &Graph) -> bool {
            if self.as_bytes(this_graph) != other.as_bytes(other_graph) {
                return false;
            }

            let real_links = &self.real_links;
            let other_real_links = &other.real_links;
            if real_links.len() != other_real_links.len() {
                return false;
            }

            for (link_a, link_b) in real_links.values().zip(other_real_links.values()) {
                if !links_equal(link_a, link_b, this_graph, other_graph) {
                    return false;
                }
            }
            true
        }

        fn as_bytes<'a>(&self, graph: &'a Graph) -> &'a [u8] {
            let head = self.head;
            let table_size = self.table_size();
            graph.data.get(head..head + table_size).unwrap()
        }
    }

    fn links_equal(link_a: &Link, link_b: &Link, graph_a: &Graph, graph_b: &Graph) -> bool {
        if !link_a.partial_equals(link_b) {
            return false;
        }

        let obj_a = link_a.obj_idx();
        let obj_b = link_b.obj_idx();
        graph_a.vertices[obj_a].equals(&graph_b.vertices[obj_b], graph_a, graph_b)
    }

    impl Graph {
        pub(crate) fn normalize(&mut self) {
            for v in &self.vertices {
                v.normalize(&mut self.data);
            }
        }
    }

    impl PartialEq for Graph {
        fn eq(&self, other: &Self) -> bool {
            self.root().equals(other.root(), self, other)
        }
    }

    pub(crate) fn extend(s: &mut Serializer, bytes: &[u8], len: usize) {
        s.embed_bytes(&bytes[0..len]).unwrap();
    }

    pub(crate) fn start_object(s: &mut Serializer, bytes: &[u8], len: usize) {
        s.push().unwrap();
        extend(s, bytes, len);
    }

    pub(crate) fn add_object(s: &mut Serializer, bytes: &[u8], len: usize, shared: bool) -> ObjIdx {
        start_object(s, bytes, len);
        s.pop_pack(shared).unwrap()
    }

    fn add_typed_offset<T: Scalar>(s: &mut Serializer, obj_idx: ObjIdx) {
        let offset_pos = s.allocate_size(T::RAW_BYTE_LEN, true).unwrap();
        s.add_link(offset_pos..offset_pos + T::RAW_BYTE_LEN, obj_idx, OffsetWhence::Head, 0, false)
            .unwrap();
    }

    pub(crate) fn add_offset(s: &mut Serializer, obj_idx: ObjIdx) {
        add_typed_offset::<Offset16>(s, obj_idx);
    }

    pub(crate) fn add_24_offset(s: &mut Serializer, obj_idx: ObjIdx) {
        add_typed_offset::<Offset24>(s, obj_idx);
    }

    pub(crate) fn add_wide_offset(s: &mut Serializer, obj_idx: ObjIdx) {
        add_typed_offset::<Offset32>(s, obj_idx);
    }

    pub(crate) fn add_virtual_offset(s: &mut Serializer, obj_idx: ObjIdx) {
        s.add_virtual_link(obj_idx).unwrap()
    }

    fn populate_serializer_complex_2(s: &mut Serializer) {
        let _ = s.start_serialize();
        let obj_5 = add_object(s, b"mn", 2, false);
        let obj_4 = add_object(s, b"jkl", 3, false);

        start_object(s, b"ghi", 3);
        add_offset(s, obj_4);
        let obj_3 = s.pop_pack(false).unwrap();

        start_object(s, b"def", 3);
        add_offset(s, obj_3);
        let obj_2 = s.pop_pack(false).unwrap();

        start_object(s, b"abc", 3);
        add_offset(s, obj_2);
        add_offset(s, obj_4);
        add_offset(s, obj_5);
        s.pop_pack(false);

        s.end_serialize();
    }

    fn populate_serializer_complex_3(s: &mut Serializer) {
        let _ = s.start_serialize();
        let obj_6 = add_object(s, b"opqrst", 6, false);
        let obj_5 = add_object(s, b"mn", 2, false);

        start_object(s, b"jkl", 3);
        add_offset(s, obj_6);
        let obj_4 = s.pop_pack(false).unwrap();

        start_object(s, b"ghi", 3);
        add_offset(s, obj_4);
        let obj_3 = s.pop_pack(false).unwrap();

        start_object(s, b"def", 3);
        add_offset(s, obj_3);
        let obj_2 = s.pop_pack(false).unwrap();

        start_object(s, b"abc", 3);
        add_offset(s, obj_2);
        add_offset(s, obj_4);
        add_offset(s, obj_5);
        s.pop_pack(false).unwrap();

        s.end_serialize();
    }

    fn populate_serializer_simple(s: &mut Serializer) {
        let _ = s.start_serialize();
        let obj_1 = add_object(s, b"ghi", 3, false);
        let obj_2 = add_object(s, b"def", 3, false);
        start_object(s, b"abc", 3);
        add_offset(s, obj_2);
        add_offset(s, obj_1);
        s.pop_pack(false);
        s.end_serialize();
    }

    pub(crate) fn populate_serializer_with_overflow(s: &mut Serializer) {
        let large_bytes = [b'a'; 50000];
        let _ = s.start_serialize();
        let obj_1 = add_object(s, &large_bytes, 10000, false);
        let obj_2 = add_object(s, &large_bytes, 20000, false);
        let obj_3 = add_object(s, &large_bytes, 50000, false);

        start_object(s, b"abc", 3);
        add_offset(s, obj_3);
        add_offset(s, obj_2);
        add_offset(s, obj_1);
        s.pop_pack(false);

        s.end_serialize();
    }

    pub(crate) fn populate_serializer_with_dedup_overflow(s: &mut Serializer) {
        let large_bytes = [b'a'; 70000];
        let _ = s.start_serialize();
        let obj_1 = add_object(s, b"def", 3, false);

        start_object(s, &large_bytes, 60000);
        add_offset(s, obj_1);
        let obj_2 = s.pop_pack(false).unwrap();

        start_object(s, &large_bytes, 10000);
        add_offset(s, obj_2);
        add_offset(s, obj_1);
        s.pop_pack(false);

        s.end_serialize();
    }

    #[test]
    fn test_graph_serialize() {
        let buf_size = 100;
        let mut s1 = Serializer::new(buf_size);
        populate_serializer_simple(&mut s1);
        let graph = Graph::from_serializer(&s1).unwrap();

        let expected_bytes = s1.copy_bytes();
        let out = graph.serialize().unwrap();
        assert_eq!(out, expected_bytes);
    }

    #[test]
    fn test_sort_shortest() {
        let buf_size = 100;
        let mut a = Serializer::new(buf_size);
        let mut e = Serializer::new(buf_size);
        populate_serializer_complex_2(&mut a);

        let mut graph = Graph::from_serializer(&a).unwrap();
        assert!(graph.sort_shortest_distance().is_ok());
        graph.normalize();

        // Expected graph
        let _ = e.start_serialize();

        let jkl = add_object(&mut e, b"jkl", 3, false);
        start_object(&mut e, b"ghi", 3);
        add_offset(&mut e, jkl);
        let ghi = e.pop_pack(false).unwrap();

        start_object(&mut e, b"def", 3);
        add_offset(&mut e, ghi);
        let def = e.pop_pack(false).unwrap();

        let mn = add_object(&mut e, b"mn", 2, false);

        start_object(&mut e, b"abc", 3);
        add_offset(&mut e, def);
        add_offset(&mut e, jkl);
        add_offset(&mut e, mn);
        e.pop_pack(false);
        e.end_serialize();

        let mut expected_graph = Graph::from_serializer(&e).unwrap();
        expected_graph.normalize();

        assert_eq!(graph, expected_graph);
    }

    #[test]
    fn test_has_overflows_1() {
        let buf_size = 100;
        let mut s = Serializer::new(buf_size);
        populate_serializer_complex_2(&mut s);
        let mut graph = Graph::from_serializer(&s).unwrap();

        assert!(!graph.has_overflows());
    }

    #[test]
    fn test_has_overflows_2() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_overflow(&mut s);
        let mut graph = Graph::from_serializer(&s).unwrap();

        assert!(graph.has_overflows());
    }

    #[test]
    fn test_has_overflows_3() {
        let buf_size = 160000;
        let mut s = Serializer::new(buf_size);
        populate_serializer_with_dedup_overflow(&mut s);
        let mut graph = Graph::from_serializer(&s).unwrap();

        assert!(graph.has_overflows());
    }

    #[test]
    fn test_duplicate_leaf() {
        let buf_size = 100;
        let mut a = Serializer::new(buf_size);
        populate_serializer_complex_2(&mut a);
        let mut graph = Graph::from_serializer(&a).unwrap();
        graph.normalize();

        let mut e = Serializer::new(buf_size);
        e.start_serialize().unwrap();

        let mn = add_object(&mut e, b"mn", 2, false);
        let jkl_2 = add_object(&mut e, b"jkl", 3, false);

        start_object(&mut e, b"ghi", 3);
        add_offset(&mut e, jkl_2);
        let ghi = e.pop_pack(false).unwrap();

        start_object(&mut e, b"def", 3);
        add_offset(&mut e, ghi);
        let def = e.pop_pack(false).unwrap();

        let jkl_1 = add_object(&mut e, b"jkl", 3, false);

        start_object(&mut e, b"abc", 3);
        add_offset(&mut e, def);
        add_offset(&mut e, jkl_1);
        add_offset(&mut e, mn);

        e.pop_pack(false).unwrap();

        let mut expected = Graph::from_serializer(&e).unwrap();
        expected.normalize();
        assert_eq!(graph, expected);
    }

    #[test]
    fn test_duplicate_interior() {
        let buf_size = 100;
        let mut c = Serializer::new(buf_size);
        populate_serializer_complex_3(&mut c);
        let mut graph = Graph::from_serializer(&c).unwrap();
        graph.duplicate_child_at_position(3, 2, 3).unwrap();

        let data_bytes = &graph.data;
        let obj_6 = &graph.vertices[6];
        assert_eq!(data_bytes.get(obj_6.head..obj_6.head + 3).unwrap(), b"jkl");
        assert_eq!(obj_6.real_links.len(), 1);
        assert_eq!(obj_6.real_links.values().next().unwrap().obj_idx(), 0);

        let obj_5 = &graph.vertices[5];
        assert_eq!(data_bytes.get(obj_5.head..obj_5.head + 3).unwrap(), b"abc");
        assert_eq!(obj_5.real_links.len(), 3);
        let child_idxes: IntSet<u32> =
            obj_5.real_links.values().map(|l| l.obj_idx() as u32).collect();
        assert!(child_idxes.contains(4));
        assert!(child_idxes.contains(2));
        assert!(child_idxes.contains(1));

        let obj_4 = &graph.vertices[4];
        assert_eq!(data_bytes.get(obj_4.head..obj_4.head + 3).unwrap(), b"def");
        assert_eq!(obj_4.real_links.len(), 1);
        assert_eq!(obj_4.real_links.values().next().unwrap().obj_idx(), 3);

        let obj_3 = &graph.vertices[3];
        assert_eq!(data_bytes.get(obj_3.head..obj_3.head + 3).unwrap(), b"ghi");
        assert_eq!(obj_3.real_links.len(), 1);
        assert_eq!(obj_3.real_links.values().next().unwrap().obj_idx(), 6);

        let obj_2 = &graph.vertices[2];
        assert_eq!(data_bytes.get(obj_2.head..obj_2.head + 3).unwrap(), b"jkl");
        assert_eq!(obj_2.real_links.len(), 1);
        assert_eq!(obj_2.real_links.values().next().unwrap().obj_idx(), 0);

        let obj_1 = &graph.vertices[1];
        assert_eq!(data_bytes.get(obj_1.head..obj_1.head + 2).unwrap(), b"mn");
        assert!(obj_1.real_links.is_empty());

        let obj_0 = &graph.vertices[0];
        assert_eq!(data_bytes.get(obj_0.head..obj_0.head + 6).unwrap(), b"opqrst");
        assert!(obj_0.real_links.is_empty());
    }

    #[test]
    fn test_shared_node_with_virtual_links() {
        let buf_size = 100;
        let mut c = Serializer::new(buf_size);

        c.start_serialize().unwrap();
        let obj_b = add_object(&mut c, b"b", 1, false);
        let obj_c = add_object(&mut c, b"c", 1, false);

        start_object(&mut c, b"d", 1);
        add_virtual_offset(&mut c, obj_b);
        let obj_d_1 = c.pop_pack(true).unwrap();

        start_object(&mut c, b"d", 1);
        add_virtual_offset(&mut c, obj_c);
        let obj_d_2 = c.pop_pack(true).unwrap();

        assert_eq!(obj_d_1, obj_d_2);

        start_object(&mut c, b"a", 1);
        add_offset(&mut c, obj_b);
        add_offset(&mut c, obj_c);
        add_offset(&mut c, obj_d_1);
        add_offset(&mut c, obj_d_2);
        c.pop_pack(true).unwrap();
        c.end_serialize();

        let graph = Graph::from_serializer(&c).unwrap();
        let d_1 = &graph.vertices[obj_d_1];
        assert_eq!(d_1.virtual_links.len(), 2);
        assert_eq!(d_1.virtual_links[0].obj_idx(), obj_b);
        assert_eq!(d_1.virtual_links[1].obj_idx(), obj_c);
    }
}
