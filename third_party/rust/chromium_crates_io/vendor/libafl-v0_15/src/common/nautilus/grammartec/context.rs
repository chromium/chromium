use alloc::{borrow::ToOwned, string::String, vec::Vec};

use hashbrown::HashMap;
use libafl_bolts::{
    nonzero,
    rands::{Rand, RomuDuoJrRand},
};
#[cfg(feature = "nautilus_py")]
use pyo3::prelude::{Py, PyAny};

use super::{
    newtypes::{NTermId, RuleId},
    rule::{Rule, RuleIdOrCustom},
    tree::Tree,
};

#[derive(Debug, Clone)]
pub struct Context {
    rules: Vec<Rule>,
    nts_to_rules: HashMap<NTermId, Vec<RuleId>>,
    nt_ids_to_name: HashMap<NTermId, String>,
    names_to_nt_id: HashMap<String, NTermId>,
    rules_to_min_size: HashMap<RuleId, usize>,

    nts_to_min_size: HashMap<NTermId, usize>,

    rules_to_num_options: HashMap<RuleId, usize>,
    nts_to_num_options: HashMap<NTermId, usize>,
    max_len: usize,
}

impl Default for Context {
    fn default() -> Self {
        Self::new()
    }
}

impl Context {
    #[must_use]
    pub fn new() -> Self {
        Context {
            rules: vec![],
            nts_to_rules: HashMap::new(),
            nt_ids_to_name: HashMap::new(),
            names_to_nt_id: HashMap::new(),

            rules_to_min_size: HashMap::new(),
            nts_to_min_size: HashMap::new(),

            rules_to_num_options: HashMap::new(),
            nts_to_num_options: HashMap::new(),
            max_len: 0,
        }
    }

    pub fn initialize(&mut self, max_len: usize) {
        self.calc_min_len();
        self.calc_num_options();
        self.max_len = max_len + 2;
    }

    #[must_use]
    pub fn get_rule(&self, r: RuleId) -> &Rule {
        let id: usize = r.into();
        &self.rules[id]
    }

    #[must_use]
    pub fn get_nt(&self, r: &RuleIdOrCustom) -> NTermId {
        self.get_rule(r.id()).nonterm()
    }

    #[must_use]
    pub fn get_num_children(&self, r: &RuleIdOrCustom) -> usize {
        self.get_rule(r.id()).number_of_nonterms()
    }

    pub fn add_rule(&mut self, nt: &str, format: &[u8]) -> RuleId {
        let rid = self.rules.len().into();
        let rule = Rule::from_format(self, nt, format);
        let ntid = self.aquire_nt_id(nt);
        self.rules.push(rule);
        self.nts_to_rules.entry(ntid).or_default().push(rid);
        rid
    }

    #[cfg(feature = "nautilus_py")]
    pub fn add_script(&mut self, nt: &str, nts: &[String], script: Py<PyAny>) -> RuleId {
        let rid = self.rules.len().into();
        let rule = Rule::from_script(self, nt, nts, script);
        let ntid = self.aquire_nt_id(nt);
        self.rules.push(rule);
        self.nts_to_rules.entry(ntid).or_default().push(rid);
        rid
    }

    pub fn add_regex(&mut self, nt: &str, regex: &str) -> RuleId {
        let rid = self.rules.len().into();
        let rule = Rule::from_regex(self, nt, regex);
        let ntid = self.aquire_nt_id(nt);
        self.rules.push(rule);
        self.nts_to_rules.entry(ntid).or_default().push(rid);
        rid
    }

    pub fn add_term_rule(&mut self, nt: &str, term: &[u8]) -> RuleId {
        let rid = self.rules.len().into();
        let ntid = self.aquire_nt_id(nt);
        self.rules.push(Rule::from_term(ntid, term));
        self.nts_to_rules.entry(ntid).or_default().push(rid);
        rid
    }

    pub fn aquire_nt_id(&mut self, nt: &str) -> NTermId {
        let next_id = self.nt_ids_to_name.len().into();
        let id = self.names_to_nt_id.entry(nt.into()).or_insert(next_id);
        self.nt_ids_to_name.entry(*id).or_insert(nt.into());
        *id
    }

    #[must_use]
    pub fn nt_id(&self, nt: &str) -> NTermId {
        *self
            .names_to_nt_id
            .get(nt)
            .unwrap_or_else(|| panic!("{}", ("no such nonterminal: ".to_owned() + nt)))
    }

    #[must_use]
    pub fn nt_id_to_s(&self, nt: NTermId) -> String {
        self.nt_ids_to_name[&nt].clone()
    }

    fn calc_min_len_for_rule(&self, r: RuleId) -> Option<usize> {
        let mut res = 1;
        for nt_id in self.get_rule(r).nonterms() {
            if let Some(min) = self.nts_to_min_size.get(nt_id) {
                //println!("Calculating length for Rule(calc_min_len_for_rule): {}, current: {}, adding: {}, because of rule: {}", self.nt_id_to_s(self.get_rule(r).nonterm().clone()), res, min, self.nt_id_to_s(nt_id.clone()));
                res += *min;
            } else {
                return None;
            }
        }
        //println!("Calculated length for Rule(calc_min_len_for_rule): {}, Length: {}", self.nt_id_to_s(self.get_rule(r).nonterm().clone()), res);
        Some(res)
    }

    pub fn calc_min_len(&mut self) {
        let mut something_changed = true;
        while something_changed {
            //TODO: find a better solution to prevent  consumed_len >= ctx.get_min_len_for_nt(*nt)' Assertions
            let mut unknown_rules = (0..self.rules.len()).map(RuleId::from).collect::<Vec<_>>();
            something_changed = false;
            while !unknown_rules.is_empty() {
                let last_len = unknown_rules.len();
                unknown_rules.retain(|rule| {
                    if let Some(min) = self.calc_min_len_for_rule(*rule) {
                        let nt = self.get_rule(*rule).nonterm();
                        //let name = self.nt_id_to_s(nt.clone()); //DEBUGGING
                        let e = self.nts_to_min_size.entry(nt).or_insert(min);
                        if *e > min {
                            *e = min;
                            something_changed = true;
                        }
                        //println!("Calculated length for Rule: {}, Length: {}, Min_length_of_nt: {}", name, min, *e);
                        self.rules_to_min_size.insert(*rule, min);
                        false
                    } else {
                        true
                    }
                });
                if last_len == unknown_rules.len() {
                    println!("Found unproductive rules: (missing base/non recursive case?)");
                    for r in unknown_rules {
                        println!("{}", self.get_rule(r).debug_show(self));
                    }
                    panic!("Broken Grammar");
                }
            }
        }
        self.calc_rule_order();
    }

    fn calc_num_options_for_rule(&self, r: RuleId) -> usize {
        let mut res = 1_usize;
        for nt_id in self.get_rule(r).nonterms() {
            res = res.saturating_mul(*self.nts_to_num_options.get(nt_id).unwrap_or(&1));
        }
        res
    }

    pub fn calc_num_options(&mut self) {
        for (nt, rules) in &self.nts_to_rules {
            self.nts_to_num_options.entry(*nt).or_insert(rules.len());
        }

        let mut something_changed = true;
        while something_changed {
            something_changed = false;

            for rid in (0..self.rules.len()).map(RuleId::from) {
                let num = self.calc_num_options_for_rule(rid);
                let nt = self.get_rule(rid).nonterm();
                let e = self.nts_to_num_options.entry(nt).or_insert(num);
                if *e < num {
                    *e = num;
                    something_changed = true;
                }
                //println!("Calculated length for Rule: {}, Length: {}, Min_length_of_nt: {}", name, min, *e);
                self.rules_to_num_options.insert(rid, num);
            }
        }
    }

    fn calc_rule_order(&mut self) {
        let rules_to_min_size = &self.rules_to_min_size;
        for rules in self.nts_to_rules.values_mut() {
            (*rules).sort_by(|r1, r2| rules_to_min_size[r1].cmp(&rules_to_min_size[r2]));
        }
    }

    #[must_use]
    pub fn check_if_nterm_has_multiple_possiblities(&self, nt: &NTermId) -> bool {
        self.get_rules_for_nt(*nt).len() > 1
    }

    pub fn get_random_len<R: Rand>(rand: &mut R, len: usize, rhs_of_rule: &[NTermId]) -> usize {
        Self::simple_get_random_len(rand, rhs_of_rule.len(), len)
    }

    //we need to get maximal sizes for all subtrees. To generate trees fairly, we want to split the
    //available size fairly to all nodes. (e.g. all children have the same expected size,
    //regardless of its index in the current rule. We use this version of the algorithm described
    //here: https://stackoverflow.com/a/8068956 to get the first value.
    fn simple_get_random_len<R: Rand>(
        rand: &mut R,
        number_of_children: usize,
        total_remaining_len: usize,
    ) -> usize {
        let mut res = total_remaining_len;
        let iters = i32::try_from(number_of_children).unwrap() - 1;
        for _ in 0..iters {
            let proposal = rand.between(0, total_remaining_len);
            if proposal < res {
                res = proposal;
            }
        }
        res
    }

    #[must_use]
    pub fn get_min_len_for_nt(&self, nt: NTermId) -> usize {
        self.nts_to_min_size[&nt]
    }

    pub fn get_random_rule_for_nt<R: Rand>(&self, rand: &mut R, nt: NTermId, len: usize) -> RuleId {
        self.simple_get_random_rule_for_nt(rand, nt, len)
    }

    pub fn get_applicable_rules<'a, R: Rand>(
        &'a self,
        rand: &'a mut R,
        max_len: usize,
        nt: NTermId,
        p_include_short_rules: usize,
    ) -> impl Iterator<Item = &'a RuleId> + 'a {
        self.nts_to_rules[&nt]
            .iter()
            .take_while(move |r| self.rules_to_min_size[*r] <= max_len)
            .filter(move |r| {
                self.rules_to_num_options[*r] > 1
                    || rand.below(nonzero!(100)) <= p_include_short_rules
            })
    }

    pub fn choose_applicable_rule<R: Rand>(
        &self,
        rand: &mut R,
        max_len: usize,
        nt: NTermId,
        p_include_short_rules: usize,
    ) -> Option<RuleId> {
        // Create a tmp rand to get around borrowing. We hardcode the fatest rand here, because why not.
        let mut rand_cpy = RomuDuoJrRand::with_seed(rand.next());
        let rules = self.get_applicable_rules(rand, max_len, nt, p_include_short_rules);
        rand_cpy.choose(rules).copied()
    }

    fn simple_get_random_rule_for_nt<R: Rand>(
        &self,
        rand: &mut R,
        nt: NTermId,
        max_len: usize,
    ) -> RuleId {
        let p_include_short_rules = 100;
        /*if self.nts_to_num_options[&nt] < 10 {
            100 * 0
        } else if max_len > 100 {
            2 * 0
        } else if max_len > 20 {
            50 * 0
        } else {
            100 * 0;
        }; */

        if let Some(opt) = self.choose_applicable_rule(rand, max_len, nt, p_include_short_rules) {
            opt
        } else if let Some(opt) = self.choose_applicable_rule(rand, max_len, nt, 100) {
            opt
        } else {
            panic!(
                "there is no way to derive {} within {} steps",
                self.nt_ids_to_name[&nt], max_len
            )
        }
    }

    #[must_use]
    pub fn get_random_len_for_ruleid(&self, _rule_id: &RuleId) -> usize {
        self.max_len //TODO?????
    }

    #[must_use]
    pub fn get_random_len_for_nt(&self, _nt: &NTermId) -> usize {
        self.max_len
    }

    #[must_use]
    pub fn get_rules_for_nt(&self, nt: NTermId) -> &Vec<RuleId> {
        &self.nts_to_rules[&nt]
    }

    pub fn generate_tree_from_nt<R: Rand>(
        &self,
        rand: &mut R,
        nt: NTermId,
        max_len: usize,
    ) -> Tree {
        let random_rule = self.get_random_rule_for_nt(rand, nt, max_len);
        self.generate_tree_from_rule(rand, random_rule, max_len - 1)
    }

    pub fn generate_tree_from_rule<R: Rand>(&self, rand: &mut R, r: RuleId, len: usize) -> Tree {
        let mut tree = Tree::from_rule_vec(vec![], self);
        tree.generate_from_rule(rand, r, len, self);
        tree
    }
}

#[cfg(test)]
mod tests {
    use alloc::{string::String, vec::Vec};

    use libafl_bolts::rands::StdRand;

    use crate::common::nautilus::grammartec::{
        context::Context,
        rule::{Rule, RuleChild, RuleIdOrCustom},
        tree::{Tree, TreeLike},
    };

    // Some (but not all) versions of clippy think the from_nt things are format strings.
    #[allow(clippy::literal_string_with_formatting_args)]
    #[test]
    fn simple_context() {
        let mut ctx = Context::new();
        let r = Rule::from_format(&mut ctx, "F", b"foo{A:a}\\{bar\\}{B:b}asd{C}");
        let soll = vec![
            RuleChild::from_lit(b"foo"),
            RuleChild::from_nt("{A:a}", &mut ctx),
            RuleChild::from_lit(b"{bar}"),
            RuleChild::from_nt("{B:b}", &mut ctx),
            RuleChild::from_lit(b"asd"),
            RuleChild::from_nt("{C}", &mut ctx),
        ];
        if let Rule::Plain(rl) = &r {
            assert_eq!(&rl.children, &soll);
        } else {
            unreachable!();
        }
        assert_eq!(r.nonterms()[0], ctx.nt_id("A"));
        assert_eq!(r.nonterms()[1], ctx.nt_id("B"));
        assert_eq!(r.nonterms()[2], ctx.nt_id("C"));
    }

    #[test]
    fn test_context() {
        let mut rand = StdRand::new();
        let mut ctx = Context::new();
        let r0 = ctx.add_rule("C", b"c{B}c");
        let r1 = ctx.add_rule("B", b"b{A}b");
        let _ = ctx.add_rule("A", b"a {A}");
        let _ = ctx.add_rule("A", b"a {A}");
        let _ = ctx.add_rule("A", b"a {A}");
        let _ = ctx.add_rule("A", b"a {A}");
        let _ = ctx.add_rule("A", b"a {A}");
        let r3 = ctx.add_rule("A", b"a");
        ctx.initialize(5);
        assert_eq!(ctx.get_min_len_for_nt(ctx.nt_id("A")), 1);
        assert_eq!(ctx.get_min_len_for_nt(ctx.nt_id("B")), 2);
        assert_eq!(ctx.get_min_len_for_nt(ctx.nt_id("C")), 3);
        let mut tree = Tree::from_rule_vec(vec![], &ctx);
        tree.generate_from_nt(&mut rand, ctx.nt_id("C"), 3, &ctx);
        assert_eq!(
            tree.rules,
            vec![
                RuleIdOrCustom::Rule(r0),
                RuleIdOrCustom::Rule(r1),
                RuleIdOrCustom::Rule(r3),
            ]
        );
        let mut data: Vec<u8> = vec![];
        tree.unparse_to(&ctx, &mut data);
        assert_eq!(String::from_utf8(data).expect("RAND_3377050372"), "cbabc");
    }

    #[test]
    fn test_generate_len() {
        let mut rand = StdRand::new();
        let mut ctx = Context::new();
        let r0 = ctx.add_rule("E", b"({E}+{E})");
        let r1 = ctx.add_rule("E", b"({E}*{E})");
        let r2 = ctx.add_rule("E", b"({E}-{E})");
        let r3 = ctx.add_rule("E", b"({E}/{E})");
        let r4 = ctx.add_rule("E", b"1");
        ctx.initialize(11);
        assert_eq!(ctx.get_min_len_for_nt(ctx.nt_id("E")), 1);

        for _ in 0..100 {
            let mut tree = Tree::from_rule_vec(vec![], &ctx);
            tree.generate_from_nt(&mut rand, ctx.nt_id("E"), 9, &ctx);
            assert!(tree.rules.len() < 10);
            assert!(!tree.rules.is_empty());
        }

        let rules = [r0, r1, r4, r4, r4]
            .iter()
            .map(|x| RuleIdOrCustom::Rule(*x))
            .collect::<Vec<_>>();
        let tree = Tree::from_rule_vec(rules, &ctx);
        let mut data: Vec<u8> = vec![];
        tree.unparse_to(&ctx, &mut data);
        assert_eq!(
            String::from_utf8(data).expect("RAND_3492562908"),
            "((1*1)+1)"
        );

        let rules = [r0, r1, r2, r3, r4, r4, r4, r4, r4]
            .iter()
            .map(|x| RuleIdOrCustom::Rule(*x))
            .collect::<Vec<_>>();
        let tree = Tree::from_rule_vec(rules, &ctx);
        let mut data: Vec<u8> = vec![];
        tree.unparse_to(&ctx, &mut data);
        assert_eq!(
            String::from_utf8(data).expect("RAND_4245419893"),
            "((((1/1)-1)*1)+1)"
        );
    }
}
