//! Input for the [`Nautilus`](https://github.com/RUB-SysSec/nautilus) grammar fuzzer methods
use alloc::{rc::Rc, string::ToString, vec::Vec};
use core::{
    cell::RefCell,
    hash::{Hash, Hasher},
};

use hashbrown::{HashMap, HashSet};
use libafl_bolts::{HasLen, ownedref::OwnedSlice};
use serde::{Deserialize, Serialize};

use crate::{
    common::nautilus::grammartec::{
        context::Context,
        newtypes::{NTermId, NodeId, RuleId},
        rule::{Rule, RuleChild, RuleIdOrCustom},
        tree::{Tree, TreeLike},
    },
    generators::nautilus::NautilusContext,
    inputs::{Input, ToTargetBytesConverter},
};

/// An [`Input`] implementation for `Nautilus` grammar.
#[derive(Serialize, Deserialize, Debug, Clone)]
pub struct NautilusInput {
    /// The input representation as Tree
    pub tree: Tree,
}

impl Input for NautilusInput {}

/// Rc Ref-cell from Input
impl From<NautilusInput> for Rc<RefCell<NautilusInput>> {
    fn from(input: NautilusInput) -> Self {
        Rc::new(RefCell::new(input))
    }
}

impl HasLen for NautilusInput {
    #[inline]
    fn len(&self) -> usize {
        self.tree.size()
    }
}

impl NautilusInput {
    /// Creates a new codes input using the given terminals
    #[must_use]
    pub fn new(tree: Tree) -> Self {
        Self { tree }
    }

    /// Create an empty [`Input`]
    #[must_use]
    pub fn empty() -> Self {
        Self {
            tree: Tree {
                rules: vec![],
                sizes: vec![],
                paren: vec![],
            },
        }
    }

    /// Generate a `Nautilus` input from the given bytes
    pub fn unparse(&self, context: &NautilusContext, bytes: &mut Vec<u8>) {
        bytes.clear();
        self.tree.unparse(NodeId::from(0), &context.ctx, bytes);
    }

    /// Get the tree representation of this input
    #[must_use]
    pub fn tree(&self) -> &Tree {
        &self.tree
    }

    /// Get the tree representation of this input, as a mutable reference
    #[must_use]
    pub fn tree_mut(&mut self) -> &mut Tree {
        &mut self.tree
    }
}

impl Hash for NautilusInput {
    fn hash<H: Hasher>(&self, state: &mut H) {
        self.tree().paren.hash(state);
        for r in &self.tree().rules {
            match r {
                RuleIdOrCustom::Custom(a, b) => {
                    a.hash(state);
                    b.hash(state);
                }
                RuleIdOrCustom::Rule(a) => a.hash(state),
            }
        }
        self.tree().sizes.hash(state);
    }
}

/// Convert from `NautilusInput` to `BytesInput`
#[derive(Debug, Clone, Copy)]
pub struct NautilusBytesConverter<'a> {
    ctx: &'a NautilusContext,
    on_error_return_empty: bool,
}

impl<'a> NautilusBytesConverter<'a> {
    #[must_use]
    /// Create a new `NautilusBytesConverter` from a context
    pub fn new(ctx: &'a NautilusContext) -> Self {
        Self {
            ctx,
            on_error_return_empty: false,
        }
    }

    #[must_use]
    /// If true, return an empty `NautilusInput` instead of an error if parsing fails
    pub fn on_error_return_empty(mut self, on_error_return_empty: bool) -> Self {
        self.on_error_return_empty = on_error_return_empty;
        self
    }
}

impl<S> ToTargetBytesConverter<NautilusInput, S> for NautilusBytesConverter<'_> {
    fn convert_to_target_bytes<'a>(
        &mut self,
        _state: &mut S,
        input: &'a NautilusInput,
    ) -> OwnedSlice<'a, u8> {
        let mut bytes = vec![];
        input.unparse(self.ctx, &mut bytes);
        OwnedSlice::from(bytes)
    }
}

type ParseResultInternal = Option<(Vec<RuleIdOrCustom>, usize)>;

struct NautilusParser<'a> {
    ctx: &'a Context,
    input: &'a [u8],
    memo: HashMap<(NTermId, usize), ParseResultInternal>,
    stack: HashSet<(NTermId, usize)>,
}

type ParseResult = Result<Option<(Vec<RuleIdOrCustom>, usize)>, libafl_bolts::Error>;

impl<'a> NautilusParser<'a> {
    fn new(ctx: &'a Context, input: &'a [u8]) -> Self {
        Self {
            ctx,
            input,
            memo: HashMap::new(),
            stack: HashSet::new(),
        }
    }

    fn parse_nt(&mut self, nt: NTermId, offset: usize) -> ParseResult {
        if let Some(res) = self.memo.get(&(nt, offset)) {
            return Ok(res.clone());
        }
        if self.stack.contains(&(nt, offset)) {
            return Ok(None);
        }
        self.stack.insert((nt, offset));

        for rule_id in self.ctx.get_rules_for_nt(nt) {
            let rule = self.ctx.get_rule(*rule_id);
            match self.parse_rule(rule, *rule_id, offset) {
                Ok(Some((nodes, consumed))) => {
                    self.stack.remove(&(nt, offset));
                    self.memo
                        .insert((nt, offset), Some((nodes.clone(), consumed)));
                    return Ok(Some((nodes, consumed)));
                }
                Ok(None) => {}
                Err(e) => return Err(e),
            }
        }

        self.stack.remove(&(nt, offset));
        self.memo.insert((nt, offset), None);
        Ok(None)
    }

    fn parse_rule(&mut self, rule: &Rule, rule_id: RuleId, offset: usize) -> ParseResult {
        match rule {
            Rule::Plain(r) => {
                let mut current_offset = offset;
                let mut nodes = vec![RuleIdOrCustom::Rule(rule_id)];

                for child in &r.children {
                    match child {
                        RuleChild::Term(t) => {
                            if self.input.get(current_offset..current_offset + t.len())
                                == Some(t.as_slice())
                            {
                                current_offset += t.len();
                            } else {
                                return Ok(None);
                            }
                        }
                        RuleChild::NTerm(nt) => {
                            if let Some((sub_nodes, consumed)) =
                                self.parse_nt(*nt, current_offset)?
                            {
                                nodes.extend(sub_nodes);
                                current_offset += consumed;
                            } else {
                                return Ok(None);
                            }
                        }
                    }
                }
                Ok(Some((nodes, current_offset - offset)))
            }
            #[cfg(feature = "regex")]
            Rule::RegExp(r) => {
                let re_str = r.hir.to_string();
                let re = regex::bytes::Regex::new(&re_str).map_err(|e| {
                    libafl_bolts::Error::illegal_argument(format!("Invalid regex: {e}"))
                })?;
                if let Some(m) = re.find_at(self.input, offset)
                    && m.start() == offset
                {
                    let len = m.len();
                    let data = self.input[offset..offset + len].to_vec();
                    return Ok(Some((vec![RuleIdOrCustom::Custom(rule_id, data)], len)));
                }
                Ok(None)
            }
            #[cfg(not(feature = "regex"))]
            Rule::RegExp(_) => Err(libafl_bolts::Error::unsupported(
                "Nautilus grammar contains RegExp rules but the 'regex' feature is disabled",
            )),
            #[cfg(feature = "nautilus_py")]
            Rule::Script(_) => Err(libafl_bolts::Error::unsupported(
                "Nautilus Python script rules are not supported for reverse parsing",
            )),
        }
    }
}

impl<S> crate::inputs::FromTargetBytesConverter<NautilusInput, S> for NautilusBytesConverter<'_> {
    fn convert_from_target_bytes(
        &mut self,
        _state: &mut S,
        bytes: &[u8],
    ) -> Result<NautilusInput, libafl_bolts::Error> {
        let start_nt = self.ctx.ctx.nt_id("START");
        let mut parser = NautilusParser::new(&self.ctx.ctx, bytes);
        let res = parser.parse_nt(start_nt, 0);
        match res {
            Ok(Some((rules, consumed))) if consumed == bytes.len() => Ok(NautilusInput::new(
                Tree::from_rule_vec(rules, &self.ctx.ctx),
            )),
            Err(e) if !self.on_error_return_empty => Err(e),
            _ => {
                if self.on_error_return_empty {
                    Ok(NautilusInput::empty())
                } else {
                    Err(libafl_bolts::Error::illegal_argument(
                        "Failed to parse bytes into NautilusInput",
                    ))
                }
            }
        }
    }
}

#[cfg(test)]
mod tests {
    use alloc::string::ToString;

    use super::{NautilusBytesConverter, NautilusContext};
    use crate::inputs::{FromTargetBytesConverter, ToTargetBytesConverter};

    #[test]
    #[cfg(feature = "nautilus")] // Nautilus parser requires nautilus feature (and regex)
    fn test_nautilus_parser() {
        // A simple grammar
        let rules = vec![
            vec!["START".to_string(), "{A}".to_string()],
            vec!["A".to_string(), "a{A}".to_string()],
            vec!["A".to_string(), "b".to_string()],
        ];
        let ctx = NautilusContext::new(10, &rules);
        let mut converter = NautilusBytesConverter::new(&ctx);

        // Test roundtrip
        let bytes = b"aab";
        let input = converter
            .convert_from_target_bytes(&mut (), bytes)
            .expect("Failed to parse");

        let out_bytes = converter.convert_to_target_bytes(&mut (), &input);
        assert_eq!(&*out_bytes, bytes);

        // Test invalid
        let bytes = b"aac";
        assert!(converter.convert_from_target_bytes(&mut (), bytes).is_err());
    }
}
