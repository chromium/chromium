use alloc::{string::String, vec::Vec};
use core::sync::atomic::AtomicBool;
use std::{fs::File, io::Write, sync::RwLock};

use hashbrown::{HashMap, HashSet};
use libafl_bolts::rands::Rand;
use serde::{Deserialize, Serialize};

use super::{
    context::Context,
    newtypes::{NTermId, NodeId, RuleId},
    rule::RuleIdOrCustom,
    tree::{Tree, TreeLike},
};

#[derive(Debug)]
pub struct ChunkStoreWrapper {
    pub chunkstore: RwLock<ChunkStore>,
    pub is_locked: AtomicBool,
}
impl ChunkStoreWrapper {
    #[must_use]
    pub fn new(work_dir: String) -> Self {
        ChunkStoreWrapper {
            chunkstore: RwLock::new(ChunkStore::new(work_dir)),
            is_locked: AtomicBool::new(false),
        }
    }
}

#[derive(Debug, Serialize, Deserialize)]
pub struct ChunkStore {
    nts_to_chunks: HashMap<NTermId, Vec<(usize, NodeId)>>,
    seen_outputs: HashSet<Vec<u8>>,
    trees: Vec<Tree>,
    work_dir: String,
    number_of_chunks: usize,
}

impl ChunkStore {
    #[must_use]
    pub fn new(work_dir: String) -> Self {
        ChunkStore {
            nts_to_chunks: HashMap::new(),
            seen_outputs: HashSet::new(),
            trees: vec![],
            work_dir,
            number_of_chunks: 0,
        }
    }

    pub fn add_tree(&mut self, tree: Tree, ctx: &Context) {
        let mut buffer = vec![];
        let id = self.trees.len();
        let mut contains_new_chunk = false;
        for i in 0..tree.size() {
            buffer.truncate(0);
            if tree.sizes[i] > 30 {
                continue;
            }
            let n = NodeId::from(i);
            tree.unparse(n, ctx, &mut buffer);
            if !self.seen_outputs.contains(&buffer) {
                self.seen_outputs.insert(buffer.clone());
                self.nts_to_chunks
                    .entry(tree.get_rule(n, ctx).nonterm())
                    .or_insert_with(Vec::new)
                    .push((id, n));
                let mut file = File::create(format!(
                    "{}/outputs/chunks/chunk_{:09}",
                    self.work_dir, self.number_of_chunks
                ))
                .expect("RAND_596689790");
                self.number_of_chunks += 1;
                file.write_all(&buffer).expect("RAND_606896756");
                contains_new_chunk = true;
            }
        }
        if contains_new_chunk {
            self.trees.push(tree);
        }
    }

    pub fn get_alternative_to<R: Rand>(
        &self,
        rand: &mut R,
        r: RuleId,
        ctx: &Context,
    ) -> Option<(&Tree, NodeId)> {
        let chunks = self
            .nts_to_chunks
            .get(&ctx.get_nt(&RuleIdOrCustom::Rule(r)));
        let relevant = chunks.map(|vec| {
            vec.iter()
                .filter(move |&&(tid, nid)| self.trees[tid].get_rule_id(nid) != r)
        });
        //The unwrap_or is just a quick and dirty fix to catch Errors from the sampler
        let selected = relevant.and_then(|iter| rand.choose(iter));
        selected.map(|&(tid, nid)| (&self.trees[tid], nid))
    }

    #[must_use]
    pub fn trees(&self) -> usize {
        self.trees.len()
    }
}

#[cfg(test)]
mod tests {
    use alloc::string::ToString;
    use std::fs;

    use libafl_bolts::rands::StdRand;

    use crate::common::nautilus::grammartec::{
        chunkstore::ChunkStore, context::Context, tree::TreeLike,
    };

    #[test]
    fn chunk_store() {
        let mut rand = StdRand::new();
        let mut ctx = Context::new();
        let r1 = ctx.add_rule("A", b"a {B:a}");
        let r2 = ctx.add_rule("B", b"b {C:a}");
        let _ = ctx.add_rule("C", b"c");
        ctx.initialize(101);
        let random_size = ctx.get_random_len_for_ruleid(&r1);
        println!("random_size: {random_size}");
        let tree = ctx.generate_tree_from_rule(&mut rand, r1, random_size);
        fs::create_dir_all("/tmp/outputs/chunks").expect("40234068");
        let mut cks = ChunkStore::new("/tmp/".to_string());
        cks.add_tree(tree, &ctx);
        // assert!(cks.seen_outputs.contains("a b c".as_bytes()));
        // assert!(cks.seen_outputs.contains("b c".as_bytes()));
        // assert!(cks.seen_outputs.contains("c".as_bytes()));
        assert_eq!(cks.nts_to_chunks[&ctx.nt_id("A")].len(), 1);
        let (tree_id, _) = cks.nts_to_chunks[&ctx.nt_id("A")][0];
        assert_eq!(cks.trees[tree_id].unparse_to_vec(&ctx), "a b c".as_bytes());

        let random_size = ctx.get_random_len_for_ruleid(&r2);
        let tree = ctx.generate_tree_from_rule(&mut rand, r2, random_size);
        cks.add_tree(tree, &ctx);
        // assert_eq!(cks.seen_outputs.len(), 3);
        // assert_eq!(cks.nts_to_chunks[&ctx.nt_id("B")].len(), 1);
        let (tree_id, node_id) = cks.nts_to_chunks[&ctx.nt_id("B")][0];
        assert_eq!(
            cks.trees[tree_id].unparse_node_to_vec(node_id, &ctx),
            "b c".as_bytes()
        );
    }
}
