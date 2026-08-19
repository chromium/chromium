//! A graph for resolving table offsets

use font_types::Uint24;

use crate::{
    object::{ObjectId, ObjectStore},
    offsets::OffsetLen,
    table_type::TableType,
    tables::layout::LookupType,
    write::TableData,
};

use std::collections::HashMap;
use std::collections::{BTreeMap, BTreeSet, BinaryHeap, HashSet, VecDeque};

#[cfg(feature = "dot2")]
mod graphviz;
mod splitting;

/// A ranking used for sorting the graph.
///
/// Nodes are assigned a space, and nodes in lower spaces are always
/// packed before nodes in higher spaces.
#[derive(Debug, Clone, Copy, PartialOrd, Ord, Hash, PartialEq, Eq)]
pub struct Space(u32);

impl Space {
    /// A generic space for nodes reachable via 16-bit offsets.
    const SHORT_REACHABLE: Space = Space(0);
    /// A generic space for nodes that are reachable via any offset.
    const REACHABLE: Space = Space(1);
    /// The first space used for assignment to specific subgraphs.
    const INIT: Space = Space(2);

    const fn is_custom(self) -> bool {
        self.0 >= Space::INIT.0
    }
}

/// A graph of subtables, starting at a single root.
///
/// This type is used during compilation, to determine the final write order
/// for the various subtables.
//NOTE: we don't derive Debug because it's way too verbose to be useful
pub struct Graph {
    /// the actual data for each table
    objects: BTreeMap<ObjectId, TableData>,
    /// graph-specific state used for sorting
    nodes: BTreeMap<ObjectId, Node>,
    order: Vec<ObjectId>,
    root: ObjectId,
    parents_invalid: bool,
    distance_invalid: bool,
    positions_invalid: bool,
    next_space: Space,
    num_roots_per_space: HashMap<Space, usize>,
}

#[derive(Debug)]
struct Node {
    size: u32,
    distance: u64,
    /// overall position after sorting
    position: u32,
    space: Space,
    parents: Vec<(ObjectId, OffsetLen)>,
    priority: Priority,
}

/// Scored used when computing shortest distance
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
struct Distance {
    // a space ranking; like rankings are packed together,
    // and larger rankings are packed after smaller ones.
    space: Space,
    distance: u64,
    // a tie-breaker, based on order within a parent
    order: u32,
}

//TODO: remove me? maybe? not really used right now...
#[derive(Debug, Clone, Copy, PartialEq, Eq, PartialOrd, Ord)]
struct Priority(u8);

/// A record of an overflowing offset
#[derive(Clone, Debug)]
pub(crate) struct Overflow {
    parent: ObjectId,
    child: ObjectId,
    distance: u32,
    offset_type: OffsetLen,
}

impl Priority {
    const ZERO: Priority = Priority(0);
    const ONE: Priority = Priority(1);
    const TWO: Priority = Priority(2);
    const THREE: Priority = Priority(3);

    #[cfg(test)]
    fn increase(&mut self) -> bool {
        let result = *self != Priority::THREE;
        self.0 = (self.0 + 1).min(3);
        result
    }
}

impl Distance {
    const ROOT: Distance = Distance {
        space: Space::SHORT_REACHABLE,
        distance: 0,
        order: 0,
    };

    fn rev(self) -> std::cmp::Reverse<Distance> {
        std::cmp::Reverse(self)
    }
}

impl Node {
    pub fn new(size: u32) -> Self {
        Node {
            //obj,
            size,
            position: Default::default(),
            distance: Default::default(),
            space: Space::REACHABLE,
            parents: Default::default(),
            priority: Default::default(),
        }
    }

    #[cfg(test)]
    fn raise_priority(&mut self) -> bool {
        self.priority.increase()
    }

    fn modified_distance(&self, order: u32) -> Distance {
        let prev_dist = self.distance as i64;
        let distance = match self.priority {
            Priority::ZERO => prev_dist,
            Priority::ONE => prev_dist - self.size as i64 / 2,
            Priority::TWO => prev_dist - self.size as i64,
            Priority::THREE => 0,
            _ => 0,
        }
        .max(0) as u64;

        Distance {
            space: self.space,
            distance,
            order,
        }
    }
}

impl Graph {
    pub(crate) fn from_obj_store(store: ObjectStore, root: ObjectId) -> Self {
        let objects = store.objects.into_iter().map(|(k, v)| (v, k)).collect();
        Self::from_objects(objects, root)
    }

    fn from_objects(objects: BTreeMap<ObjectId, TableData>, root: ObjectId) -> Self {
        let nodes = objects
            .iter()
            //TODO: ensure table sizes elsewhere?
            .map(|(key, obj)| (*key, Node::new(obj.bytes.len().try_into().unwrap())))
            .collect();
        Graph {
            objects,
            nodes,
            order: Default::default(),
            root,
            parents_invalid: true,
            distance_invalid: true,
            positions_invalid: true,
            next_space: Space::INIT,
            num_roots_per_space: Default::default(),
        }
    }

    /// Write out the serialized graph.
    ///
    /// This is not public API, and you are responsible for ensuring that
    /// the graph is sorted before calling (by calling `pack_objects`, and
    /// checking that it has succeeded).
    pub(crate) fn serialize(&self) -> Vec<u8> {
        fn write_offset(at: &mut [u8], len: OffsetLen, resolved: u32) {
            let at = &mut at[..len as u8 as usize];
            match len {
                OffsetLen::Offset16 => at.copy_from_slice(
                    u16::try_from(resolved)
                        .expect("offset overflow should be checked before now")
                        .to_be_bytes()
                        .as_slice(),
                ),
                OffsetLen::Offset24 => at.copy_from_slice(
                    Uint24::checked_new(resolved)
                        .expect("offset overflow should be checked before now")
                        .to_be_bytes()
                        .as_slice(),
                ),
                OffsetLen::Offset32 => at.copy_from_slice(resolved.to_be_bytes().as_slice()),
            }
        }

        assert!(
            !self.order.is_empty(),
            "graph must be sorted before serialization"
        );
        let mut offsets = HashMap::new();
        let mut out = Vec::new();
        let mut off = 0;

        // first pass: write out bytes, record positions of offsets
        for id in &self.order {
            let node = self.objects.get(id).unwrap();
            offsets.insert(*id, off);
            off += node.bytes.len() as u32;
            out.extend_from_slice(&node.bytes);
        }

        // second pass: write offsets
        let mut table_head = 0;
        for id in &self.order {
            let node = self.objects.get(id).unwrap();
            for offset in &node.offsets {
                let abs_off = *offsets
                    .get(&offset.object)
                    .expect("all offsets visited in first pass");
                let rel_off = abs_off - (table_head + offset.adjustment);
                let buffer_pos = table_head + offset.pos;
                let write_over = out.get_mut(buffer_pos as usize..).unwrap();
                write_offset(write_over, offset.len, rel_off);
            }
            table_head += node.bytes.len() as u32;
        }
        out
    }

    /// Attempt to pack the graph.
    ///
    /// This involves finding an order for objects such that all offsets are
    /// resolvable.
    ///
    /// In the simple case, this just means finding a topological ordering.
    /// In exceptional cases, however, this may require us to significantly
    /// modify the graph.
    ///
    /// Our implementation is closely modeled on the implementation in the
    /// HarfBuzz repacker; see the [repacker docs] for further detail.
    ///
    /// returns `true` if a solution is found, `false` otherwise
    ///
    /// [repacker docs]: https://github.com/harfbuzz/harfbuzz/blob/main/docs/repacker.md
    pub(crate) fn pack_objects(&mut self) -> bool {
        if self.basic_sort() {
            return true;
        }

        self.try_splitting_subtables();
        self.try_promoting_subtables();

        log::info!("assigning spaces");
        self.assign_spaces_hb();
        self.sort_shortest_distance();

        if !self.has_overflows() {
            return true;
        }

        // now isolate spaces in a loop, until there are no more left:
        let overflows = loop {
            let overflows = self.find_overflows();
            if overflows.is_empty() {
                // we're done
                return true;
            }
            log::trace!(
                "failed with {} overflows, current size {}",
                overflows.len(),
                self.debug_len()
            );
            if !self.try_isolating_subgraphs(&overflows) {
                log::debug!("finished isolating all subgraphs without solution");
                break overflows;
            }
            self.sort_shortest_distance();
        };

        assert!(!overflows.is_empty());
        self.debug_overflows(&overflows);
        false
    }

    /// Initial sorting operation. Attempt Kahn, falling back to shortest distance.
    ///
    /// This has to be called first, since it establishes an initial order.
    /// subsequent operations on the graph require this order.
    ///
    /// returns `true` if sort succeeds with no overflows
    fn basic_sort(&mut self) -> bool {
        log::trace!("sorting {} objects", self.objects.len());

        self.sort_kahn();
        if !self.has_overflows() {
            return true;
        }
        log::trace!("kahn failed, trying shortest distance");
        self.sort_shortest_distance();
        !self.has_overflows()
    }

    fn has_overflows(&self) -> bool {
        for (parent_id, data) in &self.objects {
            let parent = &self.nodes[parent_id];
            for link in &data.offsets {
                let child = &self.nodes[&link.object];
                //TODO: account for 'whence'
                let rel_off = child.position - parent.position;
                if link.len.max_value() < rel_off {
                    return true;
                }
            }
        }
        false
    }

    pub(crate) fn find_overflows(&self) -> Vec<Overflow> {
        let mut result = Vec::new();
        for (parent_id, data) in &self.objects {
            let parent = &self.nodes[parent_id];
            for link in &data.offsets {
                let child = &self.nodes[&link.object];
                //TODO: account for 'whence'
                let rel_off = child.position - parent.position;
                if link.len.max_value() < rel_off {
                    result.push(Overflow {
                        parent: *parent_id,
                        child: link.object,
                        distance: rel_off,
                        offset_type: link.len,
                    });
                }
            }
        }
        result
    }

    fn debug_overflows(&self, overflows: &[Overflow]) {
        let (parents, children): (HashSet<_>, HashSet<_>) =
            overflows.iter().map(|x| (x.parent, x.child)).unzip();
        log::debug!(
            "found {} overflows from {} parents to {} children",
            overflows.len(),
            parents.len(),
            children.len()
        );
        for parent in &parents {
            let lookup_type = self.objects.get(parent).unwrap().type_;
            log::debug!("obj {parent:?}: type {lookup_type}");
        }

        for overflow in overflows {
            log::trace!(
                "{:?} -> {:?} type {} dist {}",
                overflow.parent,
                overflow.child,
                overflow.offset_type,
                overflow.distance
            );
        }
    }

    // only valid if order is up to date. Returns total byte len of graph.
    fn debug_len(&self) -> usize {
        self.order
            .iter()
            .map(|id| self.objects.get(id).unwrap().bytes.len())
            .sum()
    }

    fn update_parents(&mut self) {
        if !self.parents_invalid {
            return;
        }
        for node in self.nodes.values_mut() {
            node.parents.clear();
        }

        for (id, obj) in &self.objects {
            for link in &obj.offsets {
                self.nodes
                    .get_mut(&link.object)
                    .unwrap()
                    .parents
                    .push((*id, link.len));
            }
        }
        self.parents_invalid = false;
    }

    fn remove_orphans(&mut self) {
        let mut visited = HashSet::with_capacity(self.nodes.len());
        self.find_subgraph_hb(self.root, &mut visited);
        if visited.len() != self.nodes.len() {
            log::info!("removing {} orphan nodes", self.nodes.len() - visited.len());
            for id in self
                .nodes
                .keys()
                .copied()
                .collect::<HashSet<_>>()
                .difference(&visited)
            {
                self.nodes.remove(id);
                self.objects.remove(id);
            }
        }
    }

    fn sort_kahn(&mut self) {
        self.positions_invalid = true;
        if self.nodes.len() <= 1 {
            self.order.extend(self.nodes.keys().copied());
            return;
        }

        let mut queue = BinaryHeap::new();
        let mut removed_edges = HashMap::new();
        let mut current_pos: u32 = 0;
        self.order.clear();

        self.update_parents();
        queue.push(std::cmp::Reverse(self.root));

        while let Some(id) = queue.pop().map(|x| x.0) {
            let next = &self.objects[&id];
            self.order.push(id);
            self.nodes.get_mut(&id).unwrap().position = current_pos;
            current_pos += next.bytes.len() as u32;
            for link in &next.offsets {
                let seen_edges = removed_edges.entry(link.object).or_insert(0usize);
                *seen_edges += 1;
                // if the target of this link has no other incoming links, add
                // to the queue
                if *seen_edges == self.nodes[&link.object].parents.len() {
                    queue.push(std::cmp::Reverse(link.object));
                }
            }
        }
        //TODO: check for orphans & cycles?
        for (id, seen_len) in &removed_edges {
            if *seen_len != self.nodes[id].parents.len() {
                panic!("cycle or something?");
            }
        }
    }

    pub(crate) fn sort_shortest_distance(&mut self) {
        self.positions_invalid = true;
        self.update_parents();
        self.update_distances();
        self.assign_space_0();

        let mut queue = BinaryHeap::new();
        let mut removed_edges = HashMap::with_capacity(self.nodes.len());
        let mut current_pos = 0;
        self.order.clear();

        queue.push((Distance::ROOT.rev(), self.root));
        let mut obj_order = 1u32;
        while let Some((_, id)) = queue.pop() {
            let next = &self.objects[&id];
            self.order.push(id);
            self.nodes.get_mut(&id).unwrap().position = current_pos;
            current_pos += next.bytes.len() as u32;
            for link in &next.offsets {
                let seen_edges = removed_edges.entry(link.object).or_insert(0usize);
                *seen_edges += 1;
                // if the target of this link has no other incoming links, add
                // to the queue
                if *seen_edges == self.nodes[&link.object].parents.len() {
                    let distance = self.nodes[&link.object].modified_distance(obj_order);
                    obj_order += 1;
                    queue.push((distance.rev(), link.object));
                }
            }
        }

        //TODO: check for orphans & cycles?
        for (id, seen_len) in &removed_edges {
            if *seen_len != self.nodes[id].parents.len() {
                panic!("cycle or something?");
            }
        }
    }

    /// Compute shortest distances from root using Dijkstra's algorithm.
    ///
    /// Each edge weight is `child_size + (1 << (link_width * 8))`, matching
    /// the formula in HarfBuzz's [`graph_t::update_distances`][hb-dist] and
    /// skera. The offset-width penalty (2^16 for Offset16, 2^24 for Offset24,
    /// 2^32 for Offset32) biases the sort so that children connected via
    /// narrow offsets are placed close to their parents, within the range
    /// addressable by that offset width.
    ///
    /// [hb-dist]: https://github.com/harfbuzz/harfbuzz/blob/7c110fdf/src/graph/graph.hh#L1537-L1567
    fn update_distances(&mut self) {
        self.nodes
            .values_mut()
            .for_each(|node| node.distance = u64::MAX);
        self.nodes.get_mut(&self.root).unwrap().distance = 0;

        // HarfBuzz uses a min-heap (hb_priority_queue_t::pop_minimum):
        // https://github.com/harfbuzz/harfbuzz/blob/7c110fdf/src/hb-priority-queue.hh#L34-L44
        // Rust's BinaryHeap is a max-heap, so we wrap keys in Reverse.
        let mut queue = BinaryHeap::new();
        let mut visited = HashSet::new();
        queue.push((std::cmp::Reverse(0u64), self.root));

        while let Some((_, next_id)) = queue.pop() {
            if !visited.insert(next_id) {
                continue;
            }
            let next_distance = self.nodes[&next_id].distance;
            let next_obj = &self.objects[&next_id];
            for link in &next_obj.offsets {
                if visited.contains(&link.object) {
                    continue;
                }

                let child = self.nodes.get_mut(&link.object).unwrap();
                // Match HarfBuzz's edge weight formula:
                // https://github.com/harfbuzz/harfbuzz/blob/7c110fdf/src/graph/graph.hh#L1559-L1560
                // HarfBuzz also multiplies by (space + 1), but we handle space
                // ordering via the Distance struct's space field instead.
                let link_width = link.len as u32; // 2, 3, or 4
                let child_weight = child.size as u64 + (1u64 << (link_width * 8));
                let child_distance = next_distance.saturating_add(child_weight);

                if child_distance < child.distance {
                    child.distance = child_distance;
                    queue.push((std::cmp::Reverse(child_distance), link.object));
                }
            }
        }

        self.distance_invalid = false;
    }

    /// isolate and assign spaces to subgraphs reachable via long offsets.
    ///
    /// This finds all subgraphs that are reachable via long offsets, and
    /// isolates them (ensuring they are *only* reachable via long offsets),
    /// assigning each unique space an identifier.
    ///
    /// Each space may have multiple roots; this works by finding the connected
    /// components from each root (counting only nodes reachable via long offsets).
    ///
    /// This is a close port of the [assign_spaces] method used by the HarfBuzz
    /// repacker.
    ///
    /// [assign_spaces]: https://github.com/harfbuzz/harfbuzz/blob/main/src/graph/graph.hh#L624
    fn assign_spaces_hb(&mut self) -> bool {
        self.update_parents();
        let (visited, mut roots) = self.find_space_roots_hb();

        if roots.is_empty() {
            return false;
        }

        log::trace!("found {} space roots to isolate", roots.len());

        // we want to *invert* the visited set, but we don't have a fancy hb_set_t
        let mut visited = self
            .order
            .iter()
            .copied()
            .collect::<HashSet<_>>()
            .difference(&visited)
            .copied()
            .collect::<HashSet<_>>();

        let mut connected_roots = BTreeSet::new(); // we can reuse this
        while let Some(next) = roots.iter().copied().next() {
            connected_roots.clear();
            self.find_connected_nodes_hb(next, &mut roots, &mut visited, &mut connected_roots);
            self.isolate_subgraph_hb(&mut connected_roots);

            self.distance_invalid = true;
            self.positions_invalid = true;
        }
        true
    }

    /// Find the root nodes of 32 (and later 24?)-bit space.
    ///
    /// These are the set of nodes that have incoming long offsets, for which
    /// no ancestor has an incoming long offset.
    ///
    /// Ported from the [find_space_roots] method in HarfBuzz.
    ///
    /// [find_space_roots]: https://github.com/harfbuzz/harfbuzz/blob/main/src/graph/graph.hh#L508
    fn find_space_roots_hb(&self) -> (HashSet<ObjectId>, BTreeSet<ObjectId>) {
        let mut visited = HashSet::new();
        let mut roots = BTreeSet::new();

        let mut queue = VecDeque::from([self.root]);

        while let Some(id) = queue.pop_front() {
            if visited.contains(&id) {
                continue;
            }
            let obj = self.objects.get(&id).unwrap();
            for link in &obj.offsets {
                //FIXME: harfbuzz has a bunch of logic here for 24-bit offsets
                if link.len == OffsetLen::Offset32 {
                    roots.insert(link.object);
                    self.find_subgraph_hb(link.object, &mut visited);
                } else {
                    queue.push_back(link.object);
                }
            }
        }
        (visited, roots)
    }

    fn find_subgraph_hb(&self, idx: ObjectId, nodes: &mut HashSet<ObjectId>) {
        if !nodes.insert(idx) {
            return;
        }
        for link in self.objects.get(&idx).unwrap().offsets.iter() {
            self.find_subgraph_hb(link.object, nodes);
        }
    }

    fn find_subgraph_map_hb(&self, idx: ObjectId, graph: &mut BTreeMap<ObjectId, usize>) {
        use std::collections::btree_map::Entry;
        for link in &self.objects[&idx].offsets {
            match graph.entry(link.object) {
                // To avoid double counting, we only recurse if we are seeing
                // this node for the first time.
                Entry::Vacant(entry) => {
                    entry.insert(1);
                    self.find_subgraph_map_hb(link.object, graph);
                }
                Entry::Occupied(entry) => {
                    *entry.into_mut() += 1;
                }
            }
        }
    }

    /// find all of the members of 'targets' that are reachable, skipping nodes in `visited`.
    fn find_connected_nodes_hb(
        &self,
        id: ObjectId,
        targets: &mut BTreeSet<ObjectId>,
        visited: &mut HashSet<ObjectId>,
        connected: &mut BTreeSet<ObjectId>,
    ) {
        if !visited.insert(id) {
            return;
        }
        if targets.remove(&id) {
            connected.insert(id);
        }
        // recurse to all children and parents
        for (obj, _) in &self.nodes.get(&id).unwrap().parents {
            self.find_connected_nodes_hb(*obj, targets, visited, connected);
        }
        for link in &self.objects.get(&id).unwrap().offsets {
            self.find_connected_nodes_hb(link.object, targets, visited, connected);
        }
    }

    /// Isolate the subgraph with the provided roots, moving it to a new space.
    ///
    /// This duplicates any nodes in this subgraph that are shared with
    /// any other nodes in the graph.
    ///
    /// Based on the [isolate_subgraph] method in HarfBuzz.
    ///
    /// [isolate_subgraph]: https://github.com/harfbuzz/harfbuzz/blob/main/src/graph/graph.hh#L508
    fn isolate_subgraph_hb(&mut self, roots: &mut BTreeSet<ObjectId>) -> bool {
        self.update_parents();

        // map of object id -> number of incoming edges
        let mut subgraph = BTreeMap::new();

        for root in roots.iter() {
            // for the roots, we set the edge count to the number of long
            // incoming offsets; if this differs from the total number of
            // incoming offsets it means we need to dupe the root as well.
            let inbound_wide_offsets = self.nodes[root]
                .parents
                .iter()
                .filter(|(_, len)| !matches!(len, OffsetLen::Offset16))
                .count();
            subgraph.insert(*root, inbound_wide_offsets);
            self.find_subgraph_map_hb(*root, &mut subgraph);
        }

        let next_space = self.next_space();
        log::debug!("moved {} roots to {next_space:?}", roots.len(),);
        self.num_roots_per_space.insert(next_space, roots.len());
        let mut id_map = HashMap::new();
        for (id, incoming_edges_in_subgraph) in &subgraph {
            // there are edges to this object from outside the subgraph; dupe it.
            if *incoming_edges_in_subgraph < self.nodes[id].parents.len() {
                self.duplicate_subgraph(*id, &mut id_map, next_space);
            }
        }

        // now remap any links in the subgraph from nodes that were not
        // themselves duplicated (since they were not reachable from outside)
        for id in subgraph.keys().filter(|k| !id_map.contains_key(k)) {
            self.nodes.get_mut(id).unwrap().space = next_space;
            let obj = self.objects.get_mut(id).unwrap();
            for link in &mut obj.offsets {
                if let Some(new_id) = id_map.get(&link.object) {
                    link.object = *new_id;
                }
            }
        }

        if id_map.is_empty() {
            return false;
        }

        // now everything but the links to the roots roots has been remapped;
        // remap those, if needed
        for root in roots.iter() {
            let Some(new_id) = id_map.get(root) else {
                continue;
            };
            self.parents_invalid = true;
            self.positions_invalid = true;
            for (parent_id, len) in &self.nodes[new_id].parents {
                if !matches!(len, OffsetLen::Offset16) {
                    for link in &mut self.objects.get_mut(parent_id).unwrap().offsets {
                        if link.object == *root {
                            link.object = *new_id;
                        }
                    }
                }
            }
        }

        // if any roots changed, we also rename them in the input set:
        for (old, new) in id_map {
            if roots.remove(&old) {
                roots.insert(new);
            }
        }

        true
    }

    /// for each space that has overflows and > 1 roots, select half the roots
    /// and move them to a separate subgraph.
    //
    /// return `true` if any change was made.
    ///
    /// This is a port of the [_try_isolating_subgraphs] method in hb-repacker.
    ///
    /// [_try_isolating_subgraphs]: https://github.com/harfbuzz/harfbuzz/blob/main/src/hb-repacker.hh#L182
    fn try_isolating_subgraphs(&mut self, overflows: &[Overflow]) -> bool {
        let mut to_isolate = BTreeMap::new();
        for overflow in overflows {
            let parent_space = self.nodes[&overflow.parent].space;
            // we only isolate subgraphs in wide-space
            if !parent_space.is_custom() || self.num_roots_per_space[&parent_space] < 2 {
                continue;
            }
            // if parent space is custom it means all children should also be
            // in the same custom space.
            assert_eq!(parent_space, self.nodes[&overflow.child].space);
            let root = self.find_root_of_space(overflow.parent);
            assert_eq!(self.nodes[&root].space, parent_space);
            to_isolate
                .entry(parent_space)
                .or_insert_with(BTreeSet::new)
                .insert(root);
        }

        if to_isolate.is_empty() {
            return false;
        }

        for (space, mut roots) in to_isolate {
            let n_total_roots = self.num_roots_per_space[&space];
            debug_assert!(n_total_roots >= 2, "checked in the loop above");
            let max_to_move = n_total_roots / 2;
            log::trace!(
                "moving {} of {} candidate roots from {space:?} to new space",
                max_to_move.min(roots.len()),
                roots.len()
            );
            while roots.len() > max_to_move {
                roots.pop_last();
            }
            self.isolate_subgraph_hb(&mut roots);
            *self.num_roots_per_space.get_mut(&space).unwrap() -= roots.len();
        }

        true
    }

    // invariant: obj must not be in space 0
    fn find_root_of_space(&self, obj: ObjectId) -> ObjectId {
        let space = self.nodes[&obj].space;
        debug_assert!(space.is_custom());
        let parent = self.nodes[&obj].parents[0].0;
        if self.nodes[&parent].space != space {
            return obj;
        }
        self.find_root_of_space(parent)
    }

    fn next_space(&mut self) -> Space {
        self.next_space = Space(self.next_space.0 + 1);
        self.next_space
    }

    fn try_promoting_subtables(&mut self) {
        let Some((can_promote, parent_id)) = self.get_promotable_subtables() else {
            return;
        };
        let to_promote = self.select_promotions_hb(&can_promote, parent_id);
        log::info!(
            "promoting {} of {} eligible subtables",
            to_promote.len(),
            can_promote.len()
        );
        self.actually_promote_subtables(&to_promote);
    }

    fn actually_promote_subtables(&mut self, to_promote: &[ObjectId]) {
        fn make_extension(type_: LookupType, subtable_id: ObjectId) -> TableData {
            const EXT_FORMAT: u16 = 1;
            let mut data = TableData::new(TableType::Named("ExtensionPosFormat1"));
            data.write(EXT_FORMAT);
            data.write(type_.to_raw());
            data.add_offset(subtable_id, 4, 0);
            data
        }

        for id in to_promote {
            // 'id' is a lookup table.
            // we need to:
            // - change the subtable type
            // - create a new extension table for each subtable
            // - update the object ids

            let mut lookup = self.objects.remove(id).unwrap();
            let lookup_type = lookup.type_.to_lookup_type().expect("validated before now");
            for subtable_ref in &mut lookup.offsets {
                let ext_table = make_extension(lookup_type, subtable_ref.object);
                let ext_id = self.add_object(ext_table);
                subtable_ref.object = ext_id;
            }
            lookup.write_over(lookup_type.promote().to_raw(), 0);
            lookup.type_ = lookup_type.promote().into();
            self.objects.insert(*id, lookup);
        }
        self.parents_invalid = true;
        self.positions_invalid = true;
    }

    /// Manually add an object to the graph, after initial compilation.
    ///
    /// This can be used to perform edits to the graph during compilation, such
    /// as for table splitting or promotion.
    ///
    /// This has drawbacks; in particular, at this stage we no longer deduplicate
    /// objects.
    fn add_object(&mut self, data: TableData) -> ObjectId {
        self.parents_invalid = true;
        self.distance_invalid = true;

        let id = ObjectId::next();
        self.nodes.insert(id, Node::new(data.bytes.len() as _));
        self.objects.insert(id, data);
        id
    }

    // get the list of tables that can be promoted, as well as the id of their parent table
    fn get_promotable_subtables(&self) -> Option<(Vec<ObjectId>, ObjectId)> {
        let can_promote = self
            .objects
            .iter()
            .filter_map(|(id, obj)| (obj.type_.is_promotable()).then_some(*id))
            .collect::<Vec<_>>();

        if can_promote.is_empty() {
            return None;
        }

        // sanity check: ensure that all promotable tables have a common root.
        let parents = can_promote
            .iter()
            .flat_map(|id| {
                self.nodes
                    .get(id)
                    .expect("all nodes exist")
                    .parents
                    .iter()
                    .map(|x| x.0)
            })
            .collect::<HashSet<_>>();

        // the only promotable subtables should be lookups, and there should
        // be a single LookupList that is their parent; if there is more than
        // one parent then something weird is going on.
        if parents.len() > 1 {
            if cfg!(debug_assertions) {
                panic!("Promotable subtables exist with multiple parents");
            } else {
                log::warn!("Promotable subtables exist with multiple parents");
                return None;
            }
        }

        let parent_id = *parents.iter().next().unwrap();
        Some((can_promote, parent_id))
    }

    /// select the tables to promote to extension, harfbuzz algorithm
    ///
    /// Based on the logic in HarfBuzz's [`_promote_exetnsions_if_needed`][hb-promote][hb-promote] function.
    ///
    /// [hb-promote]: https://github.com/harfbuzz/harfbuzz/blob/5d543d64222c6ce45332d0c188790f90691ef112/src/hb-repacker.hh#L97
    fn select_promotions_hb(&self, candidates: &[ObjectId], parent_id: ObjectId) -> Vec<ObjectId> {
        struct LookupSize {
            id: ObjectId,
            subgraph_size: usize,
            subtable_count: usize,
        }

        impl LookupSize {
            // I could impl Ord but then I need to impl PartialEq and it ends
            // up being way more code
            fn sort_key(&self) -> impl Ord {
                let bytes_per_subtable = self.subtable_count as f64 / self.subgraph_size as f64;
                // f64 isn't ord, so we turn it into an integer,
                // then reverse, because we want bigger things first
                std::cmp::Reverse((bytes_per_subtable * 1e9) as u64)
            }
        }

        let mut lookup_sizes = Vec::with_capacity(candidates.len());
        let mut reusable_buffer = HashSet::new();
        let mut queue = VecDeque::new();
        for id in candidates {
            // get the subgraph size
            queue.clear();
            queue.push_back(*id);
            let subgraph_size = self.find_subgraph_size(&mut queue, &mut reusable_buffer);
            let subtable_count = self.objects.get(id).unwrap().offsets.len();
            lookup_sizes.push(LookupSize {
                id: *id,
                subgraph_size,
                subtable_count,
            });
        }

        lookup_sizes.sort_by_key(LookupSize::sort_key);
        const EXTENSION_SIZE: usize = 8; // number of bytes added by an extension subtable
        const MAX_LAYER_SIZE: usize = u16::MAX as usize;

        let lookup_list_size = self.objects.get(&parent_id).unwrap().bytes.len();
        let mut l2_l3_size = lookup_list_size; // size of LookupList + lookups
        let mut l3_l4_size = 0; // Lookups + lookup subtables
        let mut l4_plus_size = 0; // subtables and anything below that

        // start by assuming all lookups are extensions; we will adjust this later
        // if we do not promote.
        for lookup in &lookup_sizes {
            let subtables_size = lookup.subtable_count * EXTENSION_SIZE;
            l3_l4_size += subtables_size;
            l4_plus_size += subtables_size;
        }

        let mut layers_full = false;
        let mut to_promote = Vec::new();
        for lookup in &lookup_sizes {
            if !layers_full {
                let lookup_size = self.objects.get(&lookup.id).unwrap().bytes.len();
                let subtables_size = self.find_children_size(lookup.id);
                let remaining_size = lookup.subgraph_size - lookup_size - subtables_size;
                l2_l3_size += lookup_size;
                l3_l4_size += lookup_size + subtables_size;
                // adjust down, because we are demoting out of extension space
                l3_l4_size -= lookup.subtable_count * EXTENSION_SIZE;
                l4_plus_size += subtables_size + remaining_size;

                if l2_l3_size < MAX_LAYER_SIZE
                    && l3_l4_size < MAX_LAYER_SIZE
                    && l4_plus_size < MAX_LAYER_SIZE
                {
                    // this lookup fits in the 16-bit space, great
                    continue;
                }
                layers_full = true;
            }
            to_promote.push(lookup.id);
        }
        to_promote
    }

    /// See if we have any subtables that support splitting, and split them
    /// if needed.
    ///
    /// Based on [`_presplit_subtables_if_needed`][presplit] in hb-repacker
    ///
    /// [presplit]: https://github.com/harfbuzz/harfbuzz/blob/5d543d64222c6ce45332d0c188790f90691ef112/src/hb-repacker.hh#LL72C22-L72C22
    fn try_splitting_subtables(&mut self) {
        let splittable = self
            .objects
            .iter()
            .filter_map(|(id, obj)| obj.type_.is_splittable().then_some(*id))
            .collect::<Vec<_>>();
        for lookup in &splittable {
            self.split_subtables_if_needed(*lookup);
        }
        if !splittable.is_empty() {
            self.remove_orphans();
        }
    }

    fn split_subtables_if_needed(&mut self, lookup: ObjectId) {
        // So You Want to Split Subtables:
        // - support PairPos and MarkBase.
        let type_ = self.objects[&lookup].type_;
        match type_ {
            TableType::GposLookup(LookupType::PAIR_POS) => splitting::split_pair_pos(self, lookup),
            TableType::GposLookup(LookupType::MARK_TO_BASE) => {
                splitting::split_mark_to_base(self, lookup)
            }
            _ => (),
        }
    }

    /// the size only of children of this object, not the whole subgraph
    fn find_children_size(&self, id: ObjectId) -> usize {
        self.objects[&id]
            .offsets
            .iter()
            .map(|off| self.objects.get(&off.object).unwrap().bytes.len())
            .sum()
    }

    fn find_subgraph_size(
        &self,
        queue: &mut VecDeque<ObjectId>,
        visited: &mut HashSet<ObjectId>,
    ) -> usize {
        let mut size = 0;
        visited.clear();
        while !queue.is_empty() {
            let next = queue.pop_front().unwrap();
            if !visited.insert(next) {
                continue;
            }
            let obj = self.objects.get(&next).unwrap();
            size += obj.bytes.len();
            queue.extend(
                obj.offsets
                    .iter()
                    .filter_map(|obj| (!visited.contains(&obj.object)).then_some(obj.object)),
            );
        }
        size
    }

    fn duplicate_subgraph(
        &mut self,
        root: ObjectId,
        dupes: &mut HashMap<ObjectId, ObjectId>,
        space: Space,
    ) -> ObjectId {
        if let Some(existing) = dupes.get(&root) {
            return *existing;
        }
        self.parents_invalid = true;
        self.distance_invalid = true;
        let new_root = ObjectId::next();
        log::trace!("duplicating node {root:?} to {new_root:?}");

        let mut obj = self.objects.get(&root).cloned().unwrap();
        let mut node = Node::new(obj.bytes.len() as u32);
        node.space = space;

        for link in &mut obj.offsets {
            // recursively duplicate the object
            link.object = self.duplicate_subgraph(link.object, dupes, space);
        }
        dupes.insert(root, new_root);
        self.objects.insert(new_root, obj);
        self.nodes.insert(new_root, node);
        new_root
    }

    /// Find the set of nodes that are reachable from root only following
    /// 16 & 24 bit offsets, and assign them to space 0.
    fn assign_space_0(&mut self) {
        let mut stack = VecDeque::from([self.root]);

        while let Some(next) = stack.pop_front() {
            match self.nodes.get_mut(&next) {
                Some(node) if node.space != Space::SHORT_REACHABLE => {
                    node.space = Space::SHORT_REACHABLE
                }
                _ => continue,
            }
            for link in self
                .objects
                .get(&next)
                .iter()
                .flat_map(|obj| obj.offsets.iter())
            {
                if link.len != OffsetLen::Offset32 {
                    stack.push_back(link.object);
                }
            }
        }
    }

    #[cfg(test)]
    fn find_descendents(&self, root: ObjectId) -> HashSet<ObjectId> {
        let mut result = HashSet::new();
        let mut stack = VecDeque::from([root]);
        while let Some(id) = stack.pop_front() {
            if result.insert(id) {
                for link in self
                    .objects
                    .get(&id)
                    .iter()
                    .flat_map(|obj| obj.offsets.iter())
                {
                    stack.push_back(link.object);
                }
            }
        }
        result
    }

    #[cfg(feature = "dot2")]
    pub(crate) fn write_graph_viz(&self, path: impl AsRef<std::path::Path>) -> std::io::Result<()> {
        // if this is set then we prune the generated graph
        const PRUNE_GRAPH_ENV_VAR: &str = "FONTC_PRUNE_GRAPH";
        let try_trim_graph = std::env::var_os(PRUNE_GRAPH_ENV_VAR).is_some();
        graphviz::GraphVizGraph::from_graph(self, try_trim_graph).write_to_file(path)
    }
}

impl Default for Priority {
    fn default() -> Self {
        Priority::ZERO
    }
}

#[cfg(test)]
mod tests {
    use std::ops::Range;

    use font_types::GlyphId16;

    use crate::TableWriter;

    use super::*;

    fn make_ids<const N: usize>() -> [ObjectId; N] {
        let mut ids = [ObjectId::next(); N];
        for id in ids.iter_mut().skip(1) {
            *id = ObjectId::next();
        }
        ids
    }

    struct Link {
        from: ObjectId,
        to: ObjectId,
        width: OffsetLen,
    }

    struct TestGraphBuilder {
        objects: Vec<(ObjectId, usize)>,
        links: Vec<Link>,
    }

    impl TestGraphBuilder {
        fn new<const N: usize>(ids: [ObjectId; N], sizes: [usize; N]) -> Self {
            TestGraphBuilder {
                objects: ids.into_iter().zip(sizes).collect(),
                links: Default::default(),
            }
        }

        fn add_link(&mut self, from: ObjectId, to: ObjectId, width: OffsetLen) -> &mut Self {
            self.links.push(Link { from, to, width });
            self
        }

        fn build(&self) -> Graph {
            let mut objects = self
                .objects
                .iter()
                .map(|(id, size)| {
                    let table = TableData::make_mock(*size);
                    (*id, table)
                })
                .collect::<BTreeMap<_, _>>();

            for link in &self.links {
                objects
                    .get_mut(&link.from)
                    .unwrap()
                    .add_mock_offset(link.to, link.width);
            }
            let root = self.objects.first().unwrap().0;
            Graph::from_objects(objects, root)
        }
    }

    //#[test]
    //fn difference_smoke_test() {
    //assert!(Distance::MIN < Distance::MAX);
    //assert!(
    //Distance::from_offset_and_size(OffsetLen::Offset16, 10)
    //< Distance::from_offset_and_size(OffsetLen::Offset16, 20)
    //);
    //assert!(
    //Distance::from_offset_and_size(OffsetLen::Offset32, 10)
    //> Distance::from_offset_and_size(OffsetLen::Offset16, 20)
    //);
    //assert!(Distance::new(10, 3) > Distance::new(10, 1));
    //}

    #[test]
    fn priority_smoke_test() {
        let mut node = Node::new(20);
        node.distance = 100;
        let mod0 = node.modified_distance(1);
        node.raise_priority();
        let mod1 = node.modified_distance(1);
        assert!(mod0 > mod1);
        node.raise_priority();
        let mod2 = node.modified_distance(1);
        assert!(mod1 > mod2);
        node.raise_priority();
        let mod3 = node.modified_distance(1);
        assert!(mod2 > mod3, "{mod2:?} {mod3:?}");

        // max priority is 3
        node.raise_priority();
        let mod4 = node.modified_distance(1);
        assert_eq!(mod3, mod4);
    }

    #[test]
    fn kahn_basic() {
        let ids = make_ids::<4>();
        let sizes = [10, 10, 20, 10];
        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset16)
            .add_link(ids[0], ids[2], OffsetLen::Offset16)
            .add_link(ids[0], ids[3], OffsetLen::Offset16)
            .add_link(ids[3], ids[1], OffsetLen::Offset16)
            .build();

        graph.sort_kahn();
        // 3 links 1, so 1 must be last
        assert_eq!(&graph.order, &[ids[0], ids[2], ids[3], ids[1]]);
    }

    #[test]
    fn shortest_basic() {
        let ids = make_ids::<4>();
        let sizes = [10, 10, 20, 10];
        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset16)
            .add_link(ids[0], ids[2], OffsetLen::Offset16)
            .add_link(ids[0], ids[3], OffsetLen::Offset16)
            .build();

        graph.sort_shortest_distance();
        // but 2 is larger than 3, so should be ordered after
        assert_eq!(&graph.order, &[ids[0], ids[1], ids[3], ids[2]]);
    }

    // Regression test: Dijkstra must use a min-heap, not a max-heap.
    //
    // When a node is reachable via two paths, a max-heap (Rust's default
    // BinaryHeap) pops the farthest node first, causing it to be visited
    // via the longer path and assigned the wrong (larger) distance.
    //
    // Layout:
    //              +--> A(5) -----+
    //              |               +--> C(100)  (shared)
    //  root(10) ---+--> B(200) ---+
    //              |          |
    //              |          +----+--> D(10)   (shared)
    //              +--> E(150) ----+
    //
    // min-heap (correct): C.dist = 5+100 = 105 (via A),
    //                     D.dist = 150+10 = 160 (via E)  -> C before D
    // max-heap (bug):     C.dist = 200+100 = 300 (via B),
    //                     D.dist = 200+10 = 210 (via B)  -> D before C
    #[test]
    fn dijkstra_min_heap() {
        let [root, a, b, c, d, e] = make_ids::<6>();
        let sizes = [10, 5, 200, 100, 10, 150];
        let mut graph = TestGraphBuilder::new([root, a, b, c, d, e], sizes)
            // root -> A, B, E
            .add_link(root, a, OffsetLen::Offset16)
            .add_link(root, b, OffsetLen::Offset16)
            .add_link(root, e, OffsetLen::Offset16)
            // A -> C (short path to C)
            .add_link(a, c, OffsetLen::Offset16)
            // B -> C (long path to C), B -> D (long path to D)
            .add_link(b, c, OffsetLen::Offset16)
            .add_link(b, d, OffsetLen::Offset16)
            // E -> D (short path to D)
            .add_link(e, d, OffsetLen::Offset16)
            .build();

        graph.sort_shortest_distance();

        // C (dist=105) must come before D (dist=160).
        // A max-heap bug would reverse this since C gets dist=300, D gets 210.
        let c_pos = graph.order.iter().position(|&id| id == c).unwrap();
        let d_pos = graph.order.iter().position(|&id| id == d).unwrap();
        assert!(
            c_pos < d_pos,
            "C (shortest dist=105) should be sorted before D (shortest dist=160), \
             got C at position {c_pos}, D at position {d_pos}"
        );
    }

    #[test]
    fn overflow_basic() {
        let ids = make_ids::<3>();
        let sizes = [10, u16::MAX as usize - 5, 100];
        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset16)
            .add_link(ids[0], ids[2], OffsetLen::Offset16)
            .add_link(ids[1], ids[2], OffsetLen::Offset16)
            .build();
        graph.sort_kahn();
        assert_eq!(graph.find_overflows().len(), 1);
        assert_eq!(graph.find_overflows()[0].parent, ids[0]);
        assert_eq!(graph.find_overflows()[0].child, ids[2]);
    }

    // Regression test: edge weights must include an offset-width penalty.
    //
    // Without the penalty `(1 << (link_width * 8))`, edge weight is just
    // `child_size` and Dijkstra produces a depth-first-ish ordering where
    // a small deep node (A1) is sorted before a larger shallow sibling (B).
    // With the penalty, each hop adds 2^16 (for Offset16), guaranteeing
    // breadth-first ordering: all depth-1 nodes sort before depth-2 nodes.
    //
    // This breadth-first ordering is critical for keeping children connected
    // via narrow (16-bit) offsets close to their parents, preventing
    // unnecessary Extension promotion in large OTL tables.
    //
    // Layout:
    //              +--> A(10) ---> A1(5)    (depth 2)
    //  root(10) ---+
    //              +--> B(100)              (depth 1)
    //
    // Without penalty: A.dist=10, A1.dist=15, B.dist=100 → A1 before B
    // With penalty:    A.dist=65546, A1.dist=131087, B.dist=65636 → B before A1
    #[test]
    fn offset_width_penalty_breadth_first() {
        let [root, a, a1, b] = make_ids::<4>();
        let sizes = [10, 10, 5, 100];
        let mut graph = TestGraphBuilder::new([root, a, a1, b], sizes)
            .add_link(root, a, OffsetLen::Offset16)
            .add_link(root, b, OffsetLen::Offset16)
            .add_link(a, a1, OffsetLen::Offset16)
            .build();

        graph.sort_shortest_distance();

        // With the offset-width penalty, B (depth 1) must be sorted before
        // A1 (depth 2). Without the penalty, A1.dist=15 < B.dist=100 and
        // A1 would incorrectly come first (depth-first ordering).
        let b_pos = graph.order.iter().position(|&id| id == b).unwrap();
        let a1_pos = graph.order.iter().position(|&id| id == a1).unwrap();
        assert!(
            b_pos < a1_pos,
            "B (depth 1) should be sorted before A1 (depth 2) with offset-width penalty, \
             got B at position {b_pos}, A1 at position {a1_pos}"
        );
    }

    #[test]
    fn duplicate_subgraph() {
        let _ = env_logger::builder().is_test(true).try_init();
        let ids = make_ids::<10>();
        let sizes = [10; 10];

        // root has two children, one 16 and one 32-bit offset.
        // those subgraphs share three nodes, which must be deduped.

        //
        //     before          after
        //      0                 0
        //     / ⑊            ┌───┘⑊
        //    1   2 ---+      1     2 ---+
        //    |\ / \   |     / \   / \   |
        //    | 3   4  5    9   3 3'  4  5
        //    |  \ / \          |  \ / \
        //    |   6   7         6   6'  7
        //    |       |                 |
        //    |    8──┘              8──┘
        //    |    │                /
        //    9 ───┘               9'

        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset16)
            .add_link(ids[0], ids[2], OffsetLen::Offset32)
            .add_link(ids[1], ids[3], OffsetLen::Offset16)
            .add_link(ids[1], ids[9], OffsetLen::Offset16)
            .add_link(ids[2], ids[3], OffsetLen::Offset16)
            .add_link(ids[2], ids[4], OffsetLen::Offset16)
            .add_link(ids[2], ids[5], OffsetLen::Offset16)
            .add_link(ids[3], ids[6], OffsetLen::Offset16)
            .add_link(ids[4], ids[6], OffsetLen::Offset16)
            .add_link(ids[4], ids[7], OffsetLen::Offset16)
            .add_link(ids[7], ids[8], OffsetLen::Offset16)
            .add_link(ids[8], ids[9], OffsetLen::Offset16)
            .build();

        assert_eq!(graph.nodes.len(), 10);
        let one = graph.find_descendents(ids[1]);
        let two = graph.find_descendents(ids[2]);
        assert_eq!(one.intersection(&two).count(), 3);

        graph.assign_spaces_hb();

        // 3, 6, and 9 should be duplicated
        assert_eq!(graph.nodes.len(), 13);
        let one = graph.find_descendents(ids[1]);
        let two = graph.find_descendents(ids[2]);
        assert_eq!(one.intersection(&two).count(), 0);

        for id in &one {
            assert!(!graph.nodes.get(id).unwrap().space.is_custom());
        }

        for id in &two {
            assert!(graph.nodes.get(id).unwrap().space.is_custom());
        }
    }

    #[test]
    fn split_overflowing_spaces() {
        // this attempts to show a simplified version of a gsub table with extension
        // subtables, before any isolation/deduplication has happened.
        //
        //    before                         after
        //      0           (GSUB)             0
        //      |                              |
        //      1        (lookup List)         1
        //      |                              |
        //      2          (Lookup)            2
        //     / \                            / \
        //  ╔═3   4═╗   (ext subtables)    ╔═3   4═╗
        //  ║       ║                      ║       ║   (long offsets)
        //  5─┐   ┌─6    (subtables)       5       6
        //  │ └─8─┘ │                     / \     / \
        //  │       │    (cov tables)    7'  8'  7   8
        //  └───7───┘
        //

        let _ = env_logger::builder().is_test(true).try_init();
        let ids = make_ids::<9>();
        // make the coverage tables big enough that overflow is unavoidable
        let sizes = [10, 4, 12, 8, 8, 14, 14, 65520, 65520];
        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset16)
            .add_link(ids[1], ids[2], OffsetLen::Offset16)
            .add_link(ids[2], ids[3], OffsetLen::Offset16)
            .add_link(ids[2], ids[4], OffsetLen::Offset16)
            .add_link(ids[3], ids[5], OffsetLen::Offset32)
            .add_link(ids[4], ids[6], OffsetLen::Offset32)
            .add_link(ids[5], ids[7], OffsetLen::Offset16)
            .add_link(ids[5], ids[8], OffsetLen::Offset16)
            .add_link(ids[6], ids[7], OffsetLen::Offset16)
            .add_link(ids[6], ids[8], OffsetLen::Offset16)
            .build();
        graph.sort_shortest_distance();

        assert!(graph.has_overflows());
        assert_eq!(graph.nodes.len(), 9);

        graph.assign_spaces_hb();
        graph.sort_shortest_distance();

        // now spaces are assigned, but not isolated
        assert_eq!(graph.nodes[&ids[5]].space, graph.nodes[&ids[6]].space);
        assert_eq!(graph.nodes.len(), 9);

        // now isolate space that overflows
        let overflows = graph.find_overflows();
        graph.try_isolating_subgraphs(&overflows);
        graph.sort_shortest_distance();

        assert_eq!(graph.nodes.len(), 11);
        assert!(graph.find_overflows().is_empty());
        // ensure we are correctly update the roots_per_space thing
        assert_eq!(graph.num_roots_per_space[&graph.nodes[&ids[6]].space], 1);
        assert_eq!(graph.num_roots_per_space[&graph.nodes[&ids[5]].space], 1);
    }

    #[test]
    fn all_roads_lead_to_overflow() {
        // this is a regression test for a bug we had where we would fail
        // to correctly duplicate shared subgraphs when there were
        // multiple links between two objects, which caused us to overcount
        // the 'incoming edges in subgraph'.

        let _ = env_logger::builder().is_test(true).try_init();

        let ids = make_ids::<9>();
        let sizes = [10, 10, 10, 10, 10, 65524, 65524, 10, 24];
        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset32)
            .add_link(ids[0], ids[2], OffsetLen::Offset32)
            .add_link(ids[0], ids[3], OffsetLen::Offset32)
            .add_link(ids[0], ids[4], OffsetLen::Offset32)
            .add_link(ids[1], ids[5], OffsetLen::Offset16)
            .add_link(ids[1], ids[5], OffsetLen::Offset16)
            .add_link(ids[2], ids[6], OffsetLen::Offset16)
            .add_link(ids[3], ids[7], OffsetLen::Offset16)
            .add_link(ids[5], ids[8], OffsetLen::Offset16)
            .add_link(ids[5], ids[8], OffsetLen::Offset16)
            .add_link(ids[6], ids[8], OffsetLen::Offset16)
            .add_link(ids[7], ids[8], OffsetLen::Offset16)
            .build();

        graph.assign_spaces_hb();
        graph.sort_shortest_distance();
        let overflows = graph.find_overflows();
        assert!(!overflows.is_empty());
        graph.try_isolating_subgraphs(&overflows);
        graph.sort_shortest_distance();
        let overflows = graph.find_overflows();
        assert!(!overflows.is_empty());
        assert!(graph.try_isolating_subgraphs(&overflows));
        graph.sort_shortest_distance();
        assert!(!graph.has_overflows());
    }

    #[test]
    fn two_roots_one_space() {
        // If a subgraph is reachable from multiple long offsets, they are all
        // initially placed in the same space.
        //
        //  ┌──0═══╗    ┌──0═══╗
        //  │  ║   ║    │  ║   ║
        //  │  ║   ║    │  ║   ║
        //  1  2   3    1  2   3
        //  │   \ /     │   \ /
        //  └────4      4    4'
        //       │      │    │
        //       5      5    5'

        let ids = make_ids::<6>();
        let sizes = [10; 6];
        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset16)
            .add_link(ids[0], ids[2], OffsetLen::Offset32)
            .add_link(ids[0], ids[3], OffsetLen::Offset32)
            .add_link(ids[1], ids[4], OffsetLen::Offset16)
            .add_link(ids[2], ids[4], OffsetLen::Offset16)
            .add_link(ids[3], ids[4], OffsetLen::Offset16)
            .add_link(ids[4], ids[5], OffsetLen::Offset16)
            .build();

        assert_eq!(graph.nodes.len(), 6);
        graph.assign_spaces_hb();
        assert_eq!(graph.nodes.len(), 8);
        let one = graph.find_descendents(ids[1]);
        assert!(one.iter().all(|id| !graph.nodes[id].space.is_custom()));

        let two = graph.find_descendents(ids[2]);
        let three = graph.find_descendents(ids[3]);
        assert_eq!(two.intersection(&three).count(), 2);
        assert_eq!(two.union(&three).count(), 4);

        assert_eq!(
            two.union(&three)
                .map(|id| graph.nodes[id].space)
                .collect::<HashSet<_>>()
                .len(),
            1
        );
    }

    #[test]
    fn duplicate_shared_root_subgraph() {
        // if a node is linked from both 16 & 32-bit space, and has no parents
        // in 32 bit space, it should always still be deduped.
        //
        //    before    after
        //     0          0
        //    / ⑊        / ⑊
        //   1   ⑊      1   2
        //   └───╴2     │
        //              2'

        let ids = make_ids::<3>();
        let sizes = [10; 3];
        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset16)
            .add_link(ids[0], ids[2], OffsetLen::Offset32)
            .add_link(ids[1], ids[2], OffsetLen::Offset16)
            .build();
        graph.assign_spaces_hb();
        assert_eq!(graph.nodes.len(), 4);
    }

    #[test]
    fn assign_space_even_without_any_duplication() {
        // the subgraph of the long offset (0->2) is already isolated, and
        // so requires no duplication; but we should still correctly assign a
        // space to the children.
        //
        //     0
        //    / ⑊
        //   1   2
        //      /
        //     3

        let ids = make_ids::<4>();
        let sizes = [10; 4];
        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset16)
            .add_link(ids[0], ids[2], OffsetLen::Offset32)
            .add_link(ids[2], ids[3], OffsetLen::Offset16)
            .build();
        graph.assign_spaces_hb();
        let two = graph.find_descendents(ids[2]);
        assert!(two.iter().all(|id| graph.nodes[id].space.is_custom()));
    }

    #[test]
    fn sort_respects_spaces() {
        let ids = make_ids::<4>();
        let sizes = [10; 4];
        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset32)
            .add_link(ids[0], ids[2], OffsetLen::Offset32)
            .add_link(ids[0], ids[3], OffsetLen::Offset16)
            .build();
        graph.sort_shortest_distance();
        assert_eq!(&graph.order, &[ids[0], ids[3], ids[1], ids[2]]);
    }

    #[test]
    fn assign_32bit_spaces_if_needed() {
        let ids = make_ids::<3>();
        let sizes = [10, u16::MAX as usize, 10];
        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset32)
            .add_link(ids[0], ids[2], OffsetLen::Offset16)
            .add_link(ids[1], ids[2], OffsetLen::Offset16)
            .build();
        graph.basic_sort();
        // this will overflow unless the 32-bit offset is put last.
        assert!(graph.has_overflows());
        graph.pack_objects();
        assert!(!graph.has_overflows());
    }

    /// Construct a real gsub table that cannot be packed unless we use extension
    /// subtables
    #[test]
    fn pack_real_gsub_table_with_extension_promotion() {
        use crate::tables::{gsub, layout};

        // trial and error: a number that just triggers overflow.
        const NUM_SUBTABLES: usize = 3279;

        // make an rsub rule for each glyph.
        let rsub_rules = (0u16..NUM_SUBTABLES as u16)
            .map(|id| {
                // Each rule will use unique coverage tables, so nothing is shared.
                let coverage = std::iter::once(GlyphId16::new(id)).collect();
                let backtrack = [id + 1, id + 3].into_iter().map(GlyphId16::new).collect();
                gsub::ReverseChainSingleSubstFormat1::new(
                    coverage,
                    vec![backtrack],
                    vec![],
                    vec![GlyphId16::new(id + 1)],
                )
            })
            .collect();

        let list = layout::LookupList::<gsub::SubstitutionLookup>::new(vec![
            gsub::SubstitutionLookup::Reverse(layout::Lookup::new(
                layout::LookupFlag::empty(),
                rsub_rules,
            )),
        ]);
        let table = gsub::Gsub::new(Default::default(), Default::default(), list);

        let mut graph = TableWriter::make_graph(&table);
        assert!(
            !graph.basic_sort(),
            "simple sorting should not resolve this graph"
        );

        const BASE_LEN: usize = 10 // GPOS header len
           + 2 // scriptlist table + featurelist (both empty, get deduped)
           + 4 // lookup list, one offset
           + 6; // lookup table (no offsets)
        const RSUB_LEN: usize = 16 // base table len
            + 6 // one-glyph coverage table
            + 8; // two-glyph backtrack coverage table

        const EXTENSION_LEN: usize = 8;

        assert_eq!(graph.debug_len(), BASE_LEN + NUM_SUBTABLES * RSUB_LEN);
        assert!(graph.pack_objects());
        assert_eq!(
            graph.debug_len(),
            BASE_LEN + NUM_SUBTABLES * RSUB_LEN + NUM_SUBTABLES * EXTENSION_LEN
        );

        const EXPECTED_N_TABLES: usize = 5 // header, script/feature/lookup lists, lookup
            - 1 // because script/feature are both empty, thus identical
            + NUM_SUBTABLES * 3 // subtable + coverage + backtrack
            + NUM_SUBTABLES; // extension table for each subtable
        assert_eq!(graph.order.len(), EXPECTED_N_TABLES);
    }

    fn make_big_pair_pos(glyph_range: Range<u16>) -> crate::tables::gpos::PositionLookup {
        use crate::tables::{gpos, layout};
        let coverage = glyph_range.clone().map(GlyphId16::new).collect();
        let pair_sets = glyph_range
            .map(|id| {
                let value_rec = gpos::ValueRecord::new().with_x_advance(id as _);
                gpos::PairSet::new(
                    // 165 is arbitrary; we want a big enough value that each
                    // PairSet is 'fairly large'.
                    (id..id + 165)
                        .map(|id2| {
                            gpos::PairValueRecord::new(
                                GlyphId16::new(id2),
                                value_rec.clone(),
                                gpos::ValueRecord::default(),
                            )
                        })
                        .collect(),
                )
            })
            .collect::<Vec<_>>();
        gpos::PositionLookup::Pair(layout::Lookup::new(
            layout::LookupFlag::empty(),
            vec![gpos::PairPos::format_1(coverage, pair_sets)],
        ))
    }

    #[test]
    fn pack_real_gpos_table_with_extension_promotion() {
        use crate::tables::{gpos, layout};

        let _ = env_logger::builder().is_test(true).try_init();

        // this is a shallow graph with large nodes, which makes it easier
        // to visualize with graphviz.
        let pp1 = make_big_pair_pos(1..20);
        let pp2 = make_big_pair_pos(100..120);
        let pp3 = make_big_pair_pos(200..221);
        let pp4 = make_big_pair_pos(400..422);
        let pp5 = make_big_pair_pos(500..523);
        let pp6 = make_big_pair_pos(600..624);
        let table = gpos::Gpos::new(
            Default::default(),
            Default::default(),
            layout::LookupList::new(vec![pp1, pp2, pp3, pp4, pp5, pp6]),
        );

        // this constructs a graph where there are overflows in a single pairpos
        // subtable.
        let mut graph = TableWriter::make_graph(&table);
        assert!(
            !graph.basic_sort(),
            "simple sorting should not resolve this graph",
        );

        // uncomment these two lines if you want to visualize the graph:

        //graph.write_graph_viz("promote_gpos_before.dot");

        let n_tables_before = graph.order.len();
        assert!(graph.pack_objects());

        //graph.write_graph_viz("promote_gpos_after.dot");

        // we should have resolved this overflow by promoting a single lookup
        // to be an extension, but our logic for determining when to promote
        // is not quite perfect, so it promotes an extra.
        //
        // if our impl changes and this is failing because we're only promoting
        // a single extension, then that's great
        assert_eq!(n_tables_before + 2, graph.order.len());
    }

    #[test]
    fn preserve_mark_filter_set_after_split() {
        use crate::tables::{gpos, layout};
        use read_fonts::tables::gpos as rgpos;
        use read_fonts::FontRead as _;

        let mut pp = make_big_pair_pos(0..100);
        let gpos::PositionLookup::Pair(ppos) = &mut pp else {
            panic!("oops");
        };
        ppos.mark_filtering_set = Some(404);
        ppos.lookup_flag |= layout::LookupFlag::USE_MARK_FILTERING_SET;
        let table = gpos::Gpos::new(
            Default::default(),
            Default::default(),
            layout::LookupList::new(vec![pp]),
        );
        let bytes = crate::dump_table(&table).unwrap();
        let rgpos = rgpos::Gpos::read(bytes.as_slice().into()).unwrap();
        let lookups = rgpos.lookup_list().unwrap();
        assert_eq!(lookups.lookup_count(), 1);
        let lookup = lookups.lookups().get(0).unwrap();
        // after splitting, the mark filter set id should be the same
        assert_eq!(lookup.mark_filtering_set(), Some(404));
        match lookup.subtables().unwrap() {
            rgpos::PositionSubtables::Pair(subtables) => assert_eq!(subtables.len(), 2),
            _ => panic!("extremely wrong"),
        };
    }

    #[test]
    fn unpackable_graph_should_fail() {
        let _ = env_logger::builder().is_test(true).try_init();
        // specifically, it should not run forever.
        let ids = make_ids::<4>();
        let sizes = [10, 10, 66000, 66000];
        let mut graph = TestGraphBuilder::new(ids, sizes)
            .add_link(ids[0], ids[1], OffsetLen::Offset32)
            .add_link(ids[1], ids[2], OffsetLen::Offset16)
            .add_link(ids[1], ids[3], OffsetLen::Offset16)
            .build();

        assert!(!graph.pack_objects());
    }

    // find_subgraph_size must count each node exactly once, even when the
    // same node is reachable via multiple parents (i.e. shared/deduplicated
    // objects).  Before the fix, a shared child would be enqueued once per
    // parent; because the visited-check only happened *after* dequeue, the
    // node's byte size was added to the total once per enqueue.
    #[test]
    fn find_subgraph_size_counts_shared_node_once() {
        // Layout:  A(10) --+--> B(20) --+
        //                  |            +--> D(30)   (shared)
        //                  +--> C(20) --+
        //
        // Correct size: 10 + 20 + 20 + 30 = 80
        // Buggy size:   10 + 20 + 20 + 30 + 30 = 110  (D counted twice)
        let ids = make_ids::<4>();
        let [a, b, c, d] = ids;
        let sizes = [10, 20, 20, 30];
        let graph = TestGraphBuilder::new(ids, sizes)
            .add_link(a, b, OffsetLen::Offset16)
            .add_link(a, c, OffsetLen::Offset16)
            .add_link(b, d, OffsetLen::Offset16)
            .add_link(c, d, OffsetLen::Offset16)
            .build();

        let mut queue = VecDeque::from([a]);
        let mut visited = HashSet::new();
        let size = graph.find_subgraph_size(&mut queue, &mut visited);
        assert_eq!(size, 80);
    }
}
