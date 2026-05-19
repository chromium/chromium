use alloc::{fmt, vec::Vec};

use hashbrown::HashMap;
use libafl_bolts::{
    Error,
    rands::{Rand, loaded_dice::LoadedDiceSampler},
};

use crate::common::nautilus::grammartec::{
    context::Context,
    newtypes::{NTermId, NodeId},
    tree::Tree,
};

pub struct RecursionInfo {
    recursive_parents: HashMap<NodeId, NodeId>,
    sampler: LoadedDiceSampler,
    depth_by_offset: Vec<usize>,
    node_by_offset: Vec<NodeId>,
}

impl fmt::Debug for RecursionInfo {
    fn fmt(&self, f: &mut fmt::Formatter<'_>) -> fmt::Result {
        f.debug_struct("RecursionInfo")
            .field("recursive_parents", &self.recursive_parents)
            .field("depth_by_offset", &self.depth_by_offset)
            .field("node_by_offset", &self.node_by_offset)
            .finish_non_exhaustive()
    }
}

impl RecursionInfo {
    #[must_use]
    pub fn new(t: &Tree, n: NTermId, ctx: &Context) -> Option<Self> {
        let (recursive_parents, node_by_offset, depth_by_offset) =
            RecursionInfo::find_parents(t, n, ctx)?;
        let sampler = RecursionInfo::build_sampler(&depth_by_offset)
            .expect("Sampler depth_by_offset invalid");
        Some(Self {
            recursive_parents,
            sampler,
            depth_by_offset,
            node_by_offset,
        })
    }

    // Constructs a tree where each node points to the first ancestor with the same nonterminal (e.g. each node points the next node above it, were the pair forms a recursive occurrence of a nonterminal).
    // This structure is an ''inverted tree''. We use it later to sample efficiently from the set
    // of all possible recursive pairs without occurring `n^2` overhead. Additionally, we return a
    // ordered vec of all nodes with nonterminal n and the depth of this node in the freshly
    // constructed 'recursion tree' (weight). Each node is the end point of exactly `weight` many
    // different recursions. Therefore we use the weight of the node to sample the endpoint of a path trough the
    // recursion tree. Then we just sample the length of this path uniformly as `(1.. weight)`. This
    // yields a uniform sample from the whole set of recursions inside the tree. If you read this, Good luck you are on your own.
    #[expect(clippy::type_complexity)]
    fn find_parents(
        t: &Tree,
        nt: NTermId,
        ctx: &Context,
    ) -> Option<(HashMap<NodeId, NodeId>, Vec<NodeId>, Vec<usize>)> {
        let mut stack = vec![(None, 0)];
        let mut res = None;
        for (i, rule) in t.rules.iter().enumerate() {
            let node = NodeId::from(i);
            let (mut maybe_parent, depth) = stack.pop().expect("RAND_3404900492");
            if ctx.get_nt(rule) == nt {
                if let Some(parent) = maybe_parent {
                    let (mut parents, mut ids, mut weights) =
                        res.unwrap_or_else(|| (HashMap::new(), vec![], vec![]));
                    parents.insert(node, parent);
                    ids.push(node);
                    weights.push(depth);
                    res = Some((parents, ids, weights));
                }
                maybe_parent = Some(node);
            }
            for _ in 0..ctx.get_num_children(rule) {
                stack.push((maybe_parent, depth + 1));
            }
        }
        res
    }

    #[expect(clippy::cast_precision_loss)]
    fn build_sampler(depths: &[usize]) -> Result<LoadedDiceSampler, Error> {
        let mut weights = depths.iter().map(|x| *x as f64).collect::<Vec<_>>();
        let norm: f64 = weights.iter().sum();
        assert!(norm > 0.0);
        for v in &mut weights {
            *v /= norm;
        }
        LoadedDiceSampler::new(&weights)
    }

    pub fn get_random_recursion_pair<R: Rand>(&mut self, rand: &mut R) -> (NodeId, NodeId) {
        let offset = self.sampler.sample(rand);
        self.get_recursion_pair_by_offset(offset)
    }

    #[must_use]
    pub fn get_recursion_pair_by_offset(&self, offset: usize) -> (NodeId, NodeId) {
        let node1 = self.node_by_offset[offset];
        let mut node2 = node1;
        for _ in 0..(self.depth_by_offset[offset]) {
            node2 = self.recursive_parents[&node1];
        }
        (node2, node1)
    }

    #[must_use]
    pub fn get_number_of_recursions(&self) -> usize {
        self.node_by_offset.len()
    }
}
