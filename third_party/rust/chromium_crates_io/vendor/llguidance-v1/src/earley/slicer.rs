use std::{collections::VecDeque, sync::Arc};

use anyhow::{ensure, Result};
use derivre::{HashMap, HashSet, RegexBuilder};

use crate::{
    derivre::Regex,
    earley::{BiasComputer, ParserRecognizer},
    toktrie::{SimpleVob, TokEnv, TokTrie, TokenId},
};

use super::parser::ITEM_TRACE;

struct TokenizerSlice {
    idx: usize,
    regex: String,
    // trie_without_child[child_idx] is a trie for all tokens in this slice, excluding the child
    trie_without_child: Vec<TokTrie>,
    trie_without_children: TokTrie,
    trie_with_children: TokTrie,
    mask_with_children: SimpleVob,
    mask_trimmed: SimpleVob,
    // each of these is a subset of the current one
    children: Vec<TokenizerSlice>,
}

impl TokenizerSlice {
    fn from_topo_node(node: &TopoNode, trie: &TokTrie, regexes: &[String]) -> Result<Self> {
        let regex = regexes[node.value].clone();
        let n_vocab = trie.vocab_size() as TokenId;
        let mut children = vec![];

        let mut mask_with_children = trie.alloc_token_set();
        if regex.is_empty() {
            mask_with_children.set_all(true);
        } else {
            let mut rx = Regex::new(&regex)
                .map_err(|e| anyhow::anyhow!("invalid regex: {:?}: {}", regex, e))?;
            for tok_idx in 0..n_vocab {
                let b = trie.token(tok_idx);
                if !b.is_empty() && rx.is_match_bytes(b) {
                    mask_with_children.allow_token(tok_idx);
                }
            }
        }

        let trie_with_children = trie.filter(&mask_with_children);
        let mut mask_without_children = mask_with_children.clone();
        let mut trie_without_child = vec![];

        for c in &node.children {
            let t = TokenizerSlice::from_topo_node(c, trie, regexes)?;

            let mut m = mask_with_children.clone();
            m.sub(&t.mask_with_children);
            trie_without_child.push(trie_with_children.filter(&m));

            mask_without_children.sub(&t.mask_with_children);

            children.push(t);
        }

        let trie_without_children = trie.filter(&mask_without_children);

        // we Or this mask with result; it's possible it's quite a bit shorter
        // than the full tokenizer size (esp. if there's lots of special tokens)
        let mut mask_trimmed = mask_with_children.clone();
        mask_trimmed.trim_trailing_zeros();

        Ok(TokenizerSlice {
            idx: node.value,
            regex,
            trie_without_child,
            trie_without_children,
            trie_with_children,
            mask_with_children,
            mask_trimmed,
            children,
        })
    }

    fn matches(&self, rec: &mut ParserRecognizer<'_>) -> bool {
        if self.regex.is_empty() {
            return false;
        }
        // set to at least 500
        let budget = 1000;
        let lexer_state = rec.lexer_state();
        let res = rec
            .lexer_mut()
            .check_subsume(lexer_state, self.idx, budget)
            .unwrap_or(false);
        if false {
            println!("slice{} {}", self.idx, res);
        }
        res
    }

    fn trie_apply(&self, rec: &mut ParserRecognizer<'_>, trg: &mut SimpleVob) {
        let t0 = crate::Instant::now();
        self.trie_with_children.add_bias(rec, trg, &[]);
        let us = t0.elapsed().as_micros() as usize;
        rec.metrics_mut().slicer_leftover_us += us;
    }

    // possibly sets bits corresponding to matching tokens in the current slice
    // returns true if it did
    fn apply(&self, rec: &mut ParserRecognizer<'_>, trg: &mut SimpleVob) -> bool {
        if self.matches(rec) {
            rec.stats_mut().slices_applied += 1;
            trg.or(&self.mask_trimmed);
            true
        } else {
            let mut num_applied = 0;
            let mut first_applied_idx = None;
            let mut applied_indices = vec![];
            for (idx, c) in self.children.iter().enumerate() {
                if c.apply(rec, trg) {
                    num_applied += 1;
                    if num_applied == 1 {
                        first_applied_idx = Some(idx);
                    } else {
                        if num_applied == 2 {
                            // we didn't do this one in the first place (to avoid allocation)
                            applied_indices.push(first_applied_idx.unwrap());
                        }
                        applied_indices.push(idx);
                    }
                }
            }

            let to_apply = match num_applied {
                // no children applied, we leave application to the caller
                0 => return false,
                // only one child applied - use the trie built exactly for this purpose
                1 => &self.trie_without_child[first_applied_idx.unwrap()],
                // otherwise apply children that have not been applied yet
                _ => {
                    // only iterate over children if we didn't apply some
                    if applied_indices.len() < self.children.len() {
                        for (idx, c) in self.children.iter().enumerate() {
                            if !applied_indices.contains(&idx) {
                                c.trie_apply(rec, trg);
                            }
                        }
                    }
                    // and then we'll apply nodes for this slice only
                    &self.trie_without_children
                }
            };

            let t0 = crate::Instant::now();
            to_apply.add_bias(rec, trg, &[]);
            let us = t0.elapsed().as_micros() as usize;
            rec.metrics_mut().slicer_leftover_us += us;

            true
        }
    }
}

pub struct SlicedBiasComputer {
    top_slice: Arc<TokenizerSlice>,
    slice_regexes: Vec<String>,
    tok_env: TokEnv,
}

const DEBUG: bool = ITEM_TRACE;
macro_rules! debug {
    ($($arg:tt)*) => {
        if cfg!(feature = "logging") && DEBUG {
            eprint!(">>> ");
            eprintln!($($arg)*);
        }
    };
}

#[derive(Debug)]
struct TopoNode {
    value: usize,
    children: Vec<TopoNode>,
}

// TODO this is stupid, but there is just a few nodes in num_nodes
// and this only runs once
// complexity O(num_nodes^3)
fn topological_sort(num_nodes: usize, edges: &HashSet<(usize, usize)>) -> Vec<TopoNode> {
    fn build_tree(
        node: usize,
        num_nodes: usize,
        edges: &HashSet<(usize, usize)>,
        visited: &mut HashSet<usize>,
    ) -> TopoNode {
        visited.insert(node);
        let children = (0..num_nodes)
            .filter(|&child| {
                edges.contains(&(child, node))
                    && !visited.contains(&child)
                    && !(0..num_nodes).any(|desc| {
                        desc != node && !visited.contains(&desc) && edges.contains(&(child, desc))
                    })
            })
            .collect::<Vec<_>>();

        TopoNode {
            value: node,
            children: children
                .iter()
                .map(|&child| build_tree(child, num_nodes, edges, visited))
                .collect(),
        }
    }

    let roots: Vec<usize> = (0..num_nodes)
        .filter(|&node| !edges.iter().any(|&(desc, _)| desc == node))
        .collect();

    let mut visited = HashSet::default();
    roots
        .iter()
        .map(|&root| build_tree(root, num_nodes, edges, &mut visited))
        .collect()
}

#[allow(dead_code)]
fn topological_sort2(num_nodes: usize, edges: &HashSet<(usize, usize)>) -> Vec<TopoNode> {
    let mut children_map: HashMap<usize, Vec<usize>> = HashMap::default();
    let mut indegree = vec![0; num_nodes];

    for &(desc, anc) in edges {
        children_map.entry(anc).or_default().push(desc);
        indegree[desc] += 1;
    }

    let mut queue = VecDeque::new();
    for node in 0..num_nodes {
        if indegree[node] == 0 {
            queue.push_back(node);
        }
    }

    let mut topo_order = vec![];
    while let Some(node) = queue.pop_front() {
        topo_order.push(node);
        for &child in children_map.get(&node).unwrap_or(&vec![]) {
            indegree[child] -= 1;
            if indegree[child] == 0 {
                queue.push_back(child);
            }
        }
    }

    let mut built_nodes: HashMap<usize, TopoNode> = HashMap::default();
    for &node in topo_order.iter().rev() {
        let children = children_map
            .get(&node)
            .unwrap_or(&vec![])
            .iter()
            .filter_map(|child| built_nodes.remove(child))
            .collect();
        built_nodes.insert(
            node,
            TopoNode {
                value: node,
                children,
            },
        );
    }

    topo_order
        .iter()
        .filter(|&&n| edges.iter().all(|&(desc, _)| desc != n))
        .filter_map(|root| built_nodes.remove(root))
        .collect()
}

impl SlicedBiasComputer {
    pub fn json_slices() -> Vec<String> {
        vec![
            r#"[\x20\x0A\x0D\x09]+"#.to_string(),
            // r#"[1-9][0-9]*"#.to_string(), - seems to make things slower
            r#"[^"\\\x00-\x1F\x7F]{1,10}"#.to_string(),
            r#"[^"\\\x00-\x1F\x7F]{1,30}"#.to_string(),
            r#"[^"\\\x00-\x1F\x7F]+"#.to_string(),
        ]
    }

    pub fn general_slices() -> Vec<String> {
        // to be improved in future
        Self::json_slices()
    }

    pub fn new(tok_env: &TokEnv, regexes: &[String]) -> Result<Self> {
        let slice_regexes = regexes.to_vec();
        let mut regexes = regexes.to_vec();
        regexes.push("".to_string());

        let roots = {
            let mut edges = HashSet::default();
            let max_fuel = 100_000;
            let mut builder = RegexBuilder::new();
            for i in 0..regexes.len() {
                for j in 0..regexes.len() {
                    if i != j
                        && (regexes[j].is_empty()
                            || builder.is_contained_in(&regexes[i], &regexes[j], max_fuel)?)
                    {
                        edges.insert((i, j));
                        // println!("edge {} {:?} ⊆ {} {:?}", i, regexes[i], j, regexes[j]);
                    }
                }
            }
            topological_sort(regexes.len(), &edges)
        };
        ensure!(roots.len() == 1, "expected only one top-slice");

        let root = TokenizerSlice::from_topo_node(&roots[0], tok_env.tok_trie(), &regexes)?;

        let r = SlicedBiasComputer {
            top_slice: Arc::new(root),
            tok_env: tok_env.clone(),
            slice_regexes,
        };

        debug!("slicer:\n{}", r.stats(false));

        Ok(r)
    }

    pub fn stats(&self, include_tokens: bool) -> String {
        let mut total_nodes = 0;
        let mut s = String::new();
        let mut todo = vec![self.top_slice.as_ref()];

        while let Some(slice) = todo.pop() {
            let trie = &slice.trie_without_children;
            total_nodes += trie.root().subtree_size();
            s.push_str(&format!(
                "slice{}: ch:{:?} /{}/ -> {}\n",
                slice.idx,
                slice.children.iter().map(|s| s.idx).collect::<Vec<_>>(),
                slice.regex,
                trie.trie_stats()
            ));
            if include_tokens {
                for (tok_idx, b) in trie.sorted_tokens() {
                    if !b.is_empty() {
                        s.push_str(&format!("  tok{}-> {}\n", tok_idx, trie.token_dbg(tok_idx)));
                    }
                }
            }
            todo.extend(slice.children.iter());
        }

        s.push_str(&format!("total_nodes: {total_nodes}\n"));
        s.push_str(&format!(
            "WILDCARD: {}\n",
            self.top_slice.trie_with_children.trie_stats()
        ));
        s
    }

    pub fn extra_lexemes(&self) -> Vec<String> {
        self.slice_regexes.clone()
    }
}

impl BiasComputer for SlicedBiasComputer {
    fn compute_bias(&self, rec: &mut ParserRecognizer<'_>, start: &[u8]) -> SimpleVob {
        let mut set = self.trie().alloc_token_set();
        let lexer_state = rec.lexer_state();
        if !self.top_slice.children.is_empty()
            && start.is_empty()
            && rec.lexer_mut().subsume_possible(lexer_state)
            && self.top_slice.apply(rec, &mut set)
        {
            // OK! applied
        } else {
            // if not top-level applied, or cannot apply, do it by hand
            self.top_slice
                .trie_with_children
                .add_bias(rec, &mut set, start);
            debug!("slicer disabled; {} tokens", set.num_set());
        }

        debug!("");

        set
    }

    fn trie(&self) -> &TokTrie {
        self.tok_env.tok_trie()
    }
}
