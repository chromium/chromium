use alloc::vec::Vec;
use core::{mem, num::NonZero};

use hashbrown::HashSet;
use libafl_bolts::{Error, rands::Rand};

use crate::common::nautilus::grammartec::{
    chunkstore::ChunkStore,
    context::Context,
    newtypes::NodeId,
    recursion_info::RecursionInfo,
    rule::RuleIdOrCustom,
    tree::{Tree, TreeLike, TreeMutation},
};

#[derive(Debug)]
pub struct Mutator {
    scratchpad: Tree,
}

impl Mutator {
    #[must_use]
    pub fn new(ctx: &Context) -> Self {
        Mutator {
            scratchpad: Tree::from_rule_vec(vec![], ctx),
        }
    }

    //Return value indicates if minimization is complete: true: complete, false: not complete
    #[expect(clippy::too_many_arguments)]
    pub fn minimize_tree<F, R: Rand>(
        &mut self,
        rand: &mut R,
        tree: &mut Tree,
        bits: &HashSet<usize>,
        ctx: &Context,
        start_index: usize,
        end_index: usize,
        tester: &mut F,
    ) -> Result<bool, Error>
    where
        F: FnMut(&TreeMutation, &HashSet<usize>, &Context) -> Result<bool, Error>,
    {
        let mut i = start_index;
        while i < tree.size() {
            let n = NodeId::from(i);
            let nt = tree.get_rule(n, ctx).nonterm();
            if tree.subtree_size(n) > ctx.get_min_len_for_nt(nt) {
                self.scratchpad
                    .generate_from_nt(rand, nt, ctx.get_min_len_for_nt(nt), ctx);
                if let Some(t) = Mutator::test_and_convert(
                    tree,
                    n,
                    &self.scratchpad,
                    NodeId::from(0),
                    ctx,
                    bits,
                    tester,
                )? {
                    let _ = mem::replace(tree, t);
                }
            }
            i += 1;
            if i == end_index {
                return Ok(false);
            }
        }
        Ok(true)
    }

    //Return value indicates if minimization is complete: true: complete, false: not complete
    pub fn minimize_rec<F>(
        &mut self,
        tree: &mut Tree,
        bits: &HashSet<usize>,
        ctx: &Context,
        start_index: usize,
        end_index: usize,
        tester: &mut F,
    ) -> Result<bool, Error>
    where
        F: FnMut(&TreeMutation, &HashSet<usize>, &Context) -> Result<bool, Error>,
    {
        let mut i = start_index;
        while i < tree.size() {
            let n = NodeId::from(i);
            if let Some(parent) = Mutator::find_parent_with_nt(tree, n, ctx) {
                if let Some(t) =
                    Mutator::test_and_convert(tree, parent, tree, n, ctx, bits, tester)?
                {
                    let _ = mem::replace(tree, t);
                    i = parent.into();
                }
            }
            i += 1;
            if i == end_index {
                return Ok(false);
            }
        }
        Ok(true)
    }

    pub fn mut_rules<F, R: Rand>(
        &mut self,
        rand: &mut R,
        tree: &Tree,
        ctx: &Context,
        start_index: usize,
        end_index: usize,
        tester: &mut F,
    ) -> Result<bool, Error>
    where
        F: FnMut(&TreeMutation, &Context) -> Result<(), Error>,
    {
        for i in start_index..end_index {
            if i == tree.size() {
                return Ok(true);
            }
            let n = NodeId::from(i);
            let old_rule_id = tree.get_rule_id(n);
            let rule_ids = ctx
                .get_rules_for_nt(ctx.get_nt(&RuleIdOrCustom::Rule(old_rule_id)))
                .clone(); //TODO: Maybe find a better solution
            for new_rule_id in rule_ids {
                if old_rule_id != new_rule_id {
                    let random_size = ctx.get_random_len_for_ruleid(&new_rule_id);
                    self.scratchpad
                        .generate_from_rule(rand, new_rule_id, random_size, ctx);
                    let repl = tree.mutate_replace_from_tree(n, &self.scratchpad, NodeId::from(0));
                    tester(&repl, ctx)?;
                }
            }
        }
        Ok(false)
    }

    pub fn mut_splice<F, R: Rand>(
        &mut self,
        rand: &mut R,
        tree: &Tree,
        ctx: &Context,
        cks: &ChunkStore,
        tester: &mut F,
    ) -> Result<(), Error>
    where
        F: FnMut(&TreeMutation, &Context) -> Result<(), Error>,
    {
        let Some(tree_size) = NonZero::new(tree.size()) else {
            return Err(Error::illegal_argument("Empty tree in mut_splice"));
        };
        let n = NodeId::from(rand.below(tree_size));
        let old_rule_id = tree.get_rule_id(n);
        if let Some((repl_tree, repl_node)) = cks.get_alternative_to(rand, old_rule_id, ctx) {
            let repl = tree.mutate_replace_from_tree(n, repl_tree, repl_node);
            tester(&repl, ctx)?;
        }
        Ok(())
    }

    //pub fn rec_splice<F>(
    //    &mut self,
    //    tree: &Tree,
    //    ctx: &Context,
    //    cks: &ChunkStore,
    //    tester: &mut F
    //    )-> Result<(), Error>
    //where
    //    F: FnMut(&TreeMutation, &Context) -> Result<(), Error>,
    //{
    //    let n = NodeId::from(rand::thread_rng().gen_range(0, tree.size()));
    //    if let Some(old_rule_id) = tree.get_rule_id(n){
    //        let nterm_id = ctx.get_rule(old_rule).nonterm();
    //        if let Some((repl_tree, repl_node)) = cks.get_alternative_to(old_rule_id, ctx) {
    //            let repl = tree.mutate_replace_from_tree(n, repl_tree, repl_node);
    //            tester(&repl, ctx)?;
    //        }
    //    }
    //
    //    return Ok(());
    //}

    pub fn mut_random<F, R: Rand>(
        &mut self,
        rand: &mut R,
        tree: &Tree,
        ctx: &Context,
        tester: &mut F,
    ) -> Result<(), Error>
    where
        F: FnMut(&TreeMutation, &Context) -> Result<(), Error>,
    {
        let Some(tree_size) = NonZero::new(tree.size()) else {
            return Err(Error::illegal_argument("Empty tree in mut_random"));
        };
        let n = NodeId::from(rand.below(tree_size));
        let nterm = tree.get_rule(n, ctx).nonterm();
        if ctx.check_if_nterm_has_multiple_possiblities(&nterm) {
            let len = ctx.get_random_len_for_nt(&nterm);
            self.scratchpad.generate_from_nt(rand, nterm, len, ctx);
            let repl = tree.mutate_replace_from_tree(n, &self.scratchpad, NodeId::from(0));
            tester(&repl, ctx)?;
        }
        Ok(())
    }

    pub fn mut_random_recursion<F, R: Rand>(
        &mut self,
        rand: &mut R,
        tree: &Tree,
        recursions: &mut Vec<RecursionInfo>,
        ctx: &Context,
        tester: &mut F,
    ) -> Result<(), Error>
    where
        F: FnMut(&TreeMutation, &Context) -> Result<(), Error>,
    {
        let max_len_of_recursions = 2 << rand.between(1, 10);
        if let Some(recursion_info) = rand.choose(recursions) {
            let recursion = recursion_info.get_random_recursion_pair(rand);
            let recursion_len_pre = recursion.1.to_i() - recursion.0.to_i();
            let recursion_len_total =
                tree.subtree_size(recursion.0) - tree.subtree_size(recursion.1);
            let recursion_len_post = recursion_len_total - recursion_len_pre;
            let num_of_recursions = max_len_of_recursions / recursion_len_total;
            //Insert pre recursion
            let postfix = tree.subtree_size(recursion.1);
            let mut rules_new = Vec::with_capacity(
                recursion_len_pre * num_of_recursions
                    + postfix
                    + recursion_len_post * num_of_recursions,
            );
            let mut sizes_new = Vec::with_capacity(
                recursion_len_pre * num_of_recursions
                    + postfix
                    + recursion_len_post * num_of_recursions,
            );
            for i in 0..num_of_recursions * recursion_len_pre {
                rules_new.push(
                    tree.get_rule_or_custom(recursion.0 + (i % recursion_len_pre))
                        .clone(),
                );
                sizes_new.push(tree.sizes[recursion.0.to_i() + (i % recursion_len_pre)]);
            }

            //Append ending of original tree
            for i in 0..postfix {
                rules_new.push(tree.get_rule_or_custom(recursion.1 + i).clone());
                sizes_new.push(tree.sizes[recursion.1.to_i() + i]);
            }

            //Adjust the sizes
            for (i, item) in sizes_new
                .iter_mut()
                .enumerate()
                .take(num_of_recursions * recursion_len_pre)
            {
                if *item >= recursion_len_pre {
                    *item += (num_of_recursions - i / recursion_len_pre - 1) * recursion_len_total;
                }
            }

            //Append post recursion
            for i in 0..num_of_recursions * recursion_len_post {
                rules_new.push(
                    tree.get_rule_or_custom(recursion.1 + postfix + (i % recursion_len_post))
                        .clone(),
                );
                sizes_new.push(tree.sizes[recursion.1.to_i() + postfix + (i % recursion_len_post)]);
            }

            let recursion_tree = Tree {
                rules: rules_new,
                sizes: sizes_new,
                paren: Vec::new(), /*paren_new*/
            };
            let repl = tree.mutate_replace_from_tree(recursion.1, &recursion_tree, NodeId::from(0));

            tester(&repl, ctx)?;
        }
        Ok(())
    }

    fn find_parent_with_nt(tree: &Tree, mut node: NodeId, ctx: &Context) -> Option<NodeId> {
        let nt = tree.get_rule(node, ctx).nonterm();
        while let Some(parent) = tree.get_parent(node) {
            if tree.get_rule(parent, ctx).nonterm() == nt {
                return Some(parent);
            }
            node = parent;
        }
        None
    }

    fn test_and_convert<F>(
        tree_a: &Tree,
        n_a: NodeId,
        tree_b: &Tree,
        n_b: NodeId,
        ctx: &Context,
        fresh_bits: &HashSet<usize>,
        tester: &mut F,
    ) -> Result<Option<Tree>, Error>
    where
        F: FnMut(&TreeMutation, &HashSet<usize>, &Context) -> Result<bool, Error>,
    {
        let repl = tree_a.mutate_replace_from_tree(n_a, tree_b, n_b);
        if tester(&repl, fresh_bits, ctx)? {
            return Ok(Some(repl.to_tree(ctx)));
        }
        Ok(None)
    }
}

#[cfg(test)]
mod tests {
    use alloc::{
        string::{String, ToString},
        vec::Vec,
    };
    use core::str;

    use hashbrown::HashSet;
    use libafl_bolts::rands::StdRand;

    use crate::{
        common::nautilus::grammartec::{
            chunkstore::ChunkStore,
            context::Context,
            newtypes::RuleId,
            rule::RuleIdOrCustom,
            tree::{Tree, TreeLike, TreeMutation},
        },
        nautilus::grammartec::mutator::Mutator,
    };

    #[test]
    fn check_mut_random_recursion() {
        let mut rand = StdRand::new();
        let r1 = RuleId::from(0);
        let r2 = RuleId::from(1);
        let r3 = RuleId::from(2);
        let r4 = RuleId::from(3);
        let r5 = RuleId::from(4);

        let mut ctx = Context::new();
        ctx.add_rule("N1", b"r1{N2}{N3}{N4}");
        ctx.add_rule("N2", b"r2");
        ctx.add_rule("N3", b"r3{N1}");
        ctx.add_rule("N1", b"r4");
        ctx.add_rule("N4", b"r5");

        let rules = [r1, r2, r3, r4, r5]
            .iter()
            .map(|x| RuleIdOrCustom::Rule(*x))
            .collect::<Vec<_>>();
        let tree = Tree::from_rule_vec(rules, &ctx);

        println!("tree: {tree:?}");
        let mut mutator = Mutator::new(&ctx);
        let mut tester = |tree_mut: &TreeMutation, _ctx: &Context| {
            println!("prefix: {:?}", tree_mut.prefix);
            println!("repl: {:?}", tree_mut.repl);
            println!("postfix: {:?}", tree_mut.postfix);
            println!("mutated tree: ");
            assert!(
                tree_mut.prefix
                    == &[r1, r2, r3]
                        .iter()
                        .map(|x| RuleIdOrCustom::Rule(*x))
                        .collect::<Vec<_>>()[..]
            );
            assert!(
                tree_mut.postfix
                    == &[r5]
                        .iter()
                        .map(|x| RuleIdOrCustom::Rule(*x))
                        .collect::<Vec<_>>()[..]
            );

            assert!(
                tree_mut.repl[0..3]
                    == [r1, r2, r3]
                        .iter()
                        .map(|x| RuleIdOrCustom::Rule(*x))
                        .collect::<Vec<_>>()[..]
            );
            assert_eq!(tree_mut.repl.last(), Some(&RuleIdOrCustom::Rule(r5)));
            Ok(())
        };
        let mut recursions = tree.calc_recursions(&ctx).expect("RAND_3407743327");
        println!("Recursions:\n{recursions:?}");
        mutator
            .mut_random_recursion(&mut rand, &tree, &mut recursions, &ctx, &mut tester)
            .expect("RAND_4227583404");
    }

    #[test]
    fn check_minimize_tree() {
        let mut rand = StdRand::new();
        let mut ctx = Context::new();
        let r1 = ctx.add_rule("S", b"s1 {A}");
        let _ = ctx.add_rule("S", b"s2");
        let _ = ctx.add_rule("S", b"a1");
        let r2 = ctx.add_rule("A", b"a1 {B}");
        let _ = ctx.add_rule("A", b"a1");
        let _ = ctx.add_rule("A", b"a2");
        let r3 = ctx.add_rule("B", b"b1");
        let _ = ctx.add_rule("B", b"b2");
        let _ = ctx.add_rule("B", b"b3{B}");
        ctx.initialize(10);
        for _ in 0..100 {
            let mut tree = Tree::from_rule_vec(
                [r1, r2, r3]
                    .iter()
                    .map(|x| RuleIdOrCustom::Rule(*x))
                    .collect::<Vec<_>>(),
                &ctx,
            );
            let mut mutator = Mutator::new(&ctx);
            {
                let mut tester =
                    |tree_mut: &TreeMutation, _bits: &HashSet<usize>, ctx: &Context| {
                        if String::from_utf8(tree_mut.unparse_to_vec(ctx))
                            .expect("RAND_2486760939")
                            .contains("a1")
                        {
                            Ok(true)
                        } else {
                            Ok(false)
                        }
                    };
                let tree_size = tree.size();
                mutator
                    .minimize_tree(
                        &mut rand,
                        &mut tree,
                        &HashSet::new(),
                        &ctx,
                        0,
                        tree_size,
                        &mut tester,
                    )
                    .expect("RAND_4046907857");
            }
            let unparse = String::from_utf8(tree.unparse_to_vec(&ctx)).expect("RAND_380778776");
            println!("unparse: {unparse}");
            assert!(unparse.contains("a1"));

            assert!(!unparse.contains("a2"));
            assert!(!unparse.contains("b2"));
            assert!(!unparse.contains("b3"));
        }
    }

    #[test]
    fn check_minimize_rec() {
        let mut ctx = Context::new();
        let r1 = ctx.add_rule("S", b"s1 {A}");
        let _ = ctx.add_rule("S", b"s2");
        let r2 = ctx.add_rule("A", b"a1 {B}");
        let _ = ctx.add_rule("A", b"a1");
        let _ = ctx.add_rule("A", b"a2");
        let r3 = ctx.add_rule("B", b"b1");
        let _ = ctx.add_rule("B", b"b2");
        let _ = ctx.add_rule("B", b"b3{B}");
        ctx.initialize(10);
        for _ in 0..100 {
            let mut tree = Tree::from_rule_vec(
                [r1, r2, r3]
                    .iter()
                    .map(|x| RuleIdOrCustom::Rule(*x))
                    .collect::<Vec<_>>(),
                &ctx,
            );
            let mut mutator = Mutator::new(&ctx);
            {
                let mut tester =
                    |tree_mut: &TreeMutation, _bits: &HashSet<usize>, ctx: &Context| {
                        if String::from_utf8(tree_mut.unparse_to_vec(ctx))
                            .expect("RAND_1958219388")
                            .contains("a1")
                        {
                            Ok(true)
                        } else {
                            Ok(false)
                        }
                    };
                let tree_size = tree.size();
                mutator
                    .minimize_rec(&mut tree, &HashSet::new(), &ctx, 0, tree_size, &mut tester)
                    .expect("RAND_1814454842");
            }
            let unparse = String::from_utf8(tree.unparse_to_vec(&ctx)).expect("RAND_3329325316");
            println!("unparse: {unparse}");
            assert!(unparse.contains("a1"));

            assert!(!unparse.contains("a2"));
            assert!(!unparse.contains("b2"));
            assert!(!unparse.contains("b3"));
        }
    }

    #[test]
    fn deterministic_rule() {
        let mut rand = StdRand::new();
        let mut ctx = Context::new();
        let r1 = ctx.add_rule("A", b"a {A:a}");
        let _ = ctx.add_rule("A", b"b {A:a}");
        let _ = ctx.add_rule("A", b"a");
        ctx.initialize(101);
        for _ in 0..100 {
            let tree = ctx.generate_tree_from_rule(&mut rand, r1, 100);
            let mut mutator = Mutator::new(&ctx);
            let unparse = tree.unparse_to_vec(&ctx);
            let mut count = 0;
            {
                let mut tester = |tree_mut: &TreeMutation, ctx: &Context| {
                    assert_ne!(tree_mut.unparse_to_vec(ctx), unparse);
                    count += 1;
                    Ok(())
                };
                mutator
                    .mut_rules(&mut rand, &tree, &ctx, 0, tree.size(), &mut tester)
                    .expect("RAND_3708258673");
            }
            assert!(count > 2);
        }
    }

    #[test]
    fn deterministic_splice() {
        let mut ctx = Context::new();
        let mut rand = StdRand::new();
        let mut cks = ChunkStore::new("/tmp/".to_string());
        let r1 = ctx.add_rule("A", b"a {A:a}");
        let _ = ctx.add_rule("A", b"b {A:a}");
        let r3 = ctx.add_rule("A", b"c {A:a}");
        let _ = ctx.add_rule("A", b"a");
        ctx.initialize(101);
        let tree = ctx.generate_tree_from_rule(&mut rand, r3, 100);
        cks.add_tree(tree, &ctx);
        for _ in 0..100 {
            let tree = ctx.generate_tree_from_rule(&mut rand, r1, 100);
            let mut mutator = Mutator::new(&ctx);
            let unparse = tree.unparse_to_vec(&ctx);
            let mut tester = |tree_mut: &TreeMutation, ctx: &Context| {
                assert_ne!(tree_mut.unparse_to_vec(ctx), unparse);
                Ok(())
            };
            mutator
                .mut_splice(&mut rand, &tree, &ctx, &cks, &mut tester)
                .expect("RAND_236145345");
        }
    }

    #[test]
    fn check_det_rules_values() {
        let mut rand = StdRand::new();
        let mut ctx = Context::new();
        let r1 = ctx.add_rule("S", b"s1 {A}");
        let _ = ctx.add_rule("S", b"s2 {A}");
        let r2 = ctx.add_rule("A", b"a1 {B}");
        let _ = ctx.add_rule("A", b"a2 {B}");
        let r3 = ctx.add_rule("B", b"b1");
        let _ = ctx.add_rule("B", b"b2");
        ctx.initialize(10);
        for _ in 0..100 {
            let tree = Tree::from_rule_vec(
                [r1, r2, r3]
                    .iter()
                    .map(|x| RuleIdOrCustom::Rule(*x))
                    .collect::<Vec<_>>(),
                &ctx,
            );
            let mut mutator = Mutator::new(&ctx);
            let mut unparses = HashSet::new();
            {
                let mut tester = |tree_mut: &TreeMutation, ctx: &Context| {
                    unparses.insert(tree_mut.unparse_to_vec(ctx));
                    Ok(())
                };
                mutator
                    .mut_rules(&mut rand, &tree, &ctx, 0, tree.size(), &mut tester)
                    .expect("RAND_3954705736");
            }
            println!(
                "{:?}",
                unparses
                    .iter()
                    .map(|v| str::from_utf8(v).expect("RAND_3927087882"))
                    .collect::<Vec<_>>()
            );
            assert!(unparses.contains("s1 a1 b2".as_bytes()));

            assert!(
                unparses.contains("s1 a2 b1".as_bytes())
                    || unparses.contains("s1 a2 b2".as_bytes())
            );

            assert!(
                unparses.contains("s2 a1 b1".as_bytes())
                    || unparses.contains("s2 a2 b2".as_bytes())
                    || unparses.contains("s2 a1 b2".as_bytes())
                    || unparses.contains("s2 a2 b1".as_bytes())
            );
        }
    }
}
