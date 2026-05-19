use alloc::{string::String, vec::Vec};
use std::sync::OnceLock;

use libafl_bolts::rands::Rand;
#[cfg(feature = "nautilus_py")]
use pyo3::prelude::{Py, PyAny, Python};
use regex_syntax::hir::Hir;
use serde::{Deserialize, Serialize};

use crate::common::nautilus::{
    grammartec::{
        context::Context,
        newtypes::{NTermId, NodeId, RuleId},
        tree::Tree,
    },
    regex_mutator,
};

#[derive(Debug, PartialEq, Eq, Clone, Serialize, Deserialize)]
pub enum RuleChild {
    Term(Vec<u8>),
    NTerm(NTermId),
}

static SPLITTER: OnceLock<regex::Regex> = OnceLock::new();
static TOKENIZER: OnceLock<regex::bytes::Regex> = OnceLock::new();

fn show_bytes(bs: &[u8]) -> String {
    use core::{ascii::escape_default, str};

    let mut visible = String::new();
    for &b in bs {
        let part: Vec<u8> = escape_default(b).collect();
        visible.push_str(str::from_utf8(&part).unwrap());
    }
    format!("\"{visible}\"")
}

impl RuleChild {
    #[must_use]
    pub fn from_lit(lit: &[u8]) -> Self {
        RuleChild::Term(lit.into())
    }

    pub fn from_nt(nt: &str, ctx: &mut Context) -> Self {
        let (nonterm, _) = RuleChild::split_nt_description(nt);
        RuleChild::NTerm(ctx.aquire_nt_id(&nonterm))
    }

    fn split_nt_description(nonterm: &str) -> (String, String) {
        let splitter = SPLITTER.get_or_init(|| {
            regex::Regex::new(r"^\{([A-Z][a-zA-Z_\-0-9]*)(?::([a-zA-Z_\-0-9]*))?\}$")
                .expect("RAND_1363289094")
        });

        //splits {A:a} or {A} into A and maybe a
        let descr = splitter.captures(nonterm).unwrap_or_else(|| panic!("could not interpret Nonterminal {nonterm:?}. Nonterminal Descriptions need to match start with a capital letter and con only contain [a-zA-Z_-0-9]"));
        //let name = descr.get(2).map(|m| m.as_str().into()).unwrap_or(default.to_string()));
        (descr[1].into(), String::new())
    }

    fn debug_show(&self, ctx: &Context) -> String {
        match self {
            RuleChild::Term(d) => show_bytes(d),
            RuleChild::NTerm(nt) => ctx.nt_id_to_s(*nt),
        }
    }
}

#[derive(Debug, Clone, Eq, PartialEq, Serialize, Deserialize)]
pub enum RuleIdOrCustom {
    Rule(RuleId),
    Custom(RuleId, Vec<u8>),
}
impl RuleIdOrCustom {
    #[must_use]
    pub fn id(&self) -> RuleId {
        match self {
            RuleIdOrCustom::Rule(id) | RuleIdOrCustom::Custom(id, _) => *id,
        }
    }

    #[must_use]
    pub fn data(&self) -> &[u8] {
        match self {
            RuleIdOrCustom::Custom(_, data) => data,
            RuleIdOrCustom::Rule(_) => panic!("cannot get data on a normal rule"),
        }
    }
}

#[derive(Debug, Clone)]
pub enum Rule {
    Plain(PlainRule),
    #[cfg(feature = "nautilus_py")]
    Script(ScriptRule),
    RegExp(RegExpRule),
}

#[derive(Debug, Clone)]
pub struct RegExpRule {
    pub nonterm: NTermId,
    pub hir: Hir,
}

impl RegExpRule {
    #[must_use]
    pub fn debug_show(&self, ctx: &Context) -> String {
        format!("{} => {:?}", ctx.nt_id_to_s(self.nonterm), self.hir)
    }
}

#[cfg(feature = "nautilus_py")]
#[derive(Debug)]
pub struct ScriptRule {
    pub nonterm: NTermId,
    pub nonterms: Vec<NTermId>,
    pub script: Py<PyAny>,
}

#[cfg(feature = "nautilus_py")]
impl ScriptRule {
    #[must_use]
    pub fn debug_show(&self, ctx: &Context) -> String {
        let args = self
            .nonterms
            .iter()
            .map(|nt| ctx.nt_id_to_s(*nt))
            .collect::<Vec<_>>()
            .join(", ");
        format!("{} => func({args})", ctx.nt_id_to_s(self.nonterm))
    }
}

#[derive(Debug, Clone, Serialize, Deserialize, PartialEq)]
pub struct PlainRule {
    pub nonterm: NTermId,
    pub children: Vec<RuleChild>,
    pub nonterms: Vec<NTermId>,
}

impl PlainRule {
    #[must_use]
    pub fn debug_show(&self, ctx: &Context) -> String {
        let args = self
            .children
            .iter()
            .map(|child| child.debug_show(ctx))
            .collect::<Vec<_>>()
            .join(", ");
        format!("{} => {args}", ctx.nt_id_to_s(self.nonterm))
    }
}

#[cfg(feature = "nautilus_py")]
impl Clone for ScriptRule {
    fn clone(&self) -> Self {
        Python::attach(|py| ScriptRule {
            nonterm: self.nonterm,
            nonterms: self.nonterms.clone(),
            script: self.script.clone_ref(py),
        })
    }
}

impl Rule {
    #[cfg(feature = "nautilus_py")]
    pub fn from_script(
        ctx: &mut Context,
        nonterm: &str,
        nterms: &[String],
        script: Py<PyAny>,
    ) -> Self {
        Self::Script(ScriptRule {
            nonterm: ctx.aquire_nt_id(nonterm),
            nonterms: nterms.iter().map(|s| ctx.aquire_nt_id(s)).collect(),
            script,
        })
    }

    pub fn from_regex(ctx: &mut Context, nonterm: &str, regex: &str) -> Self {
        use regex_syntax::ParserBuilder;

        let mut parser = ParserBuilder::new().unicode(true).utf8(false).build();

        let hir = parser.parse(regex).unwrap();

        Self::RegExp(RegExpRule {
            nonterm: ctx.aquire_nt_id(nonterm),
            hir,
        })
    }

    #[must_use]
    pub fn debug_show(&self, ctx: &Context) -> String {
        match self {
            Self::Plain(r) => r.debug_show(ctx),
            #[cfg(feature = "nautilus_py")]
            Self::Script(r) => r.debug_show(ctx),
            Self::RegExp(r) => r.debug_show(ctx),
        }
    }

    pub fn from_format(ctx: &mut Context, nonterm: &str, format: &[u8]) -> Self {
        let children = Rule::tokenize(format, ctx);
        let nonterms = children
            .iter()
            .filter_map(|c| {
                if let &RuleChild::NTerm(n) = c {
                    Some(n)
                } else {
                    None
                }
            })
            .collect();
        Self::Plain(PlainRule {
            nonterm: ctx.aquire_nt_id(nonterm),
            children,
            nonterms,
        })
    }

    #[must_use]
    pub fn from_term(ntermid: NTermId, term: &[u8]) -> Self {
        let children = vec![RuleChild::Term(term.to_vec())];
        let nonterms = vec![];
        Self::Plain(PlainRule {
            nonterm: ntermid,
            children,
            nonterms,
        })
    }

    fn unescape(bytes: &[u8]) -> Vec<u8> {
        if bytes.len() < 2 {
            return bytes.to_vec();
        }
        let mut res = vec![];
        let mut i = 0;
        while i < bytes.len() - 1 {
            if bytes[i] == 92 && bytes[i + 1] == 123 {
                // replace \{ with {
                res.push(123);
                i += 1;
            } else if bytes[i] == 92 && bytes[i + 1] == 125 {
                // replace \} with }
                res.push(125);
                i += 1;
            } else {
                res.push(bytes[i]);
            }
            i += 1;
        }
        if i < bytes.len() {
            res.push(bytes[bytes.len() - 1]);
        }
        res
    }

    fn tokenize(format: &[u8], ctx: &mut Context) -> Vec<RuleChild> {
        let tokenizer = TOKENIZER.get_or_init(|| {
            regex::bytes::RegexBuilder::new(r"(?-u)(\{[^}\\]+\})|((?:[^{\\]|\\\{|\\\}|\\)+)")
                .dot_matches_new_line(true)
                .build()
                .expect("RAND_994455541")
            // RegExp Changed from (\{[^}\\]+\})|((?:[^{\\]|\\\{|\\\}|\\\\)+) because of problems with \\ (\\ was not matched and therefore thrown away)
        });

        tokenizer
            .captures_iter(format)
            .map(|cap| {
                if let Some(sub) = cap.get(1) {
                    //println!("cap.get(1): {}", sub.as_str());
                    RuleChild::from_nt(
                        core::str::from_utf8(sub.as_bytes())
                            .expect("nonterminals need to be valid strings"),
                        ctx,
                    )
                } else if let Some(sub) = cap.get(2) {
                    RuleChild::from_lit(&Self::unescape(sub.as_bytes()))
                } else {
                    unreachable!()
                }
            })
            .collect::<Vec<_>>()
    }

    #[must_use]
    pub fn nonterms(&self) -> &[NTermId] {
        match self {
            #[cfg(feature = "nautilus_py")]
            Rule::Script(r) => &r.nonterms,
            Rule::Plain(r) => &r.nonterms,
            Rule::RegExp(_) => &[],
        }
    }

    #[must_use]
    pub fn number_of_nonterms(&self) -> usize {
        self.nonterms().len()
    }

    #[must_use]
    pub fn nonterm(&self) -> NTermId {
        match self {
            #[cfg(feature = "nautilus_py")]
            Rule::Script(r) => r.nonterm,
            Rule::Plain(r) => r.nonterm,
            Rule::RegExp(r) => r.nonterm,
        }
    }

    pub fn generate<R: Rand>(
        &self,
        rand: &mut R,
        tree: &mut Tree,
        ctx: &Context,
        len: usize,
    ) -> usize {
        // println!("Rhs: {:?}, len: {}", self.nonterms, len);
        // println!("Min needed len: {}", self.nonterms.iter().fold(0, |sum, nt| sum + ctx.get_min_len_for_nt(*nt) ));
        let minimal_needed_len = self
            .nonterms()
            .iter()
            .fold(0, |sum, nt| sum + ctx.get_min_len_for_nt(*nt));
        assert!(minimal_needed_len <= len);
        let mut remaining_len = len;
        remaining_len -= minimal_needed_len;

        //if we have no further children, we consumed no len
        let mut total_size = 1;
        let paren = NodeId::from(tree.rules.len() - 1);
        //generate each childs tree from the left to the right. That way the only operation we ever
        //perform is to push another node to the end of the tree_vec

        for (i, nt) in self.nonterms().iter().enumerate() {
            //sample how much len this child can use up (e.g. how big can
            //let cur_child_max_len = Rule::get_random_len(remaining_nts, remaining_len) + ctx.get_min_len_for_nt(*nt);
            let mut cur_child_max_len;
            let mut new_nterms = Vec::new();
            new_nterms.extend_from_slice(&self.nonterms()[i..]);
            if new_nterms.is_empty() {
                cur_child_max_len = remaining_len;
            } else {
                cur_child_max_len = Context::get_random_len(rand, remaining_len, &new_nterms);
            }
            cur_child_max_len += ctx.get_min_len_for_nt(*nt);

            //get a rule that can be used with the remaining length
            let rid = ctx.get_random_rule_for_nt(rand, *nt, cur_child_max_len);
            let rule_or_custom = match ctx.get_rule(rid) {
                Rule::Plain(_) => RuleIdOrCustom::Rule(rid),
                #[cfg(feature = "nautilus_py")]
                Rule::Script(_) => RuleIdOrCustom::Rule(rid),
                Rule::RegExp(RegExpRule { hir, .. }) => {
                    RuleIdOrCustom::Custom(rid, regex_mutator::generate(rand, hir))
                }
            };

            assert_eq!(tree.rules.len(), tree.sizes.len());
            assert_eq!(tree.sizes.len(), tree.paren.len());
            let offset = tree.rules.len();

            tree.rules.push(rule_or_custom);
            tree.sizes.push(0);
            tree.paren.push(NodeId::from(0));

            //generate the subtree for this rule, return the total consumed len
            let consumed_len = ctx
                .get_rule(rid)
                .generate(rand, tree, ctx, cur_child_max_len - 1);
            tree.sizes[offset] = consumed_len;
            tree.paren[offset] = paren;

            //println!("{}: min_needed_len: {}, Min-len: {} Consumed len: {} cur_child_max_len: {} remaining len: {}, total_size: {}, len: {}", ctx.nt_id_to_s(nt.clone()), minimal_needed_len, ctx.get_min_len_for_nt(*nt), consumed_len, cur_child_max_len, remaining_len, total_size, len);
            assert!(consumed_len <= cur_child_max_len);

            //println!("Rule: {}, min_len: {}", ctx.nt_id_to_s(nt.clone()), ctx.get_min_len_for_nt(*nt));
            assert!(consumed_len >= ctx.get_min_len_for_nt(*nt));

            //we can use the len that where not consumed by this iteration during the next iterations,
            //therefore it will be redistributed evenly amongst the other

            remaining_len += ctx.get_min_len_for_nt(*nt);

            remaining_len -= consumed_len;
            //add the consumed len to the total_len
            total_size += consumed_len;
        }
        //println!("Rule: {}, Size: {}", ctx.nt_id_to_s(self.nonterm.clone()), total_size);
        total_size
    }
}
