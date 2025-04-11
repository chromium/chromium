use anyhow::{bail, Result};
use regex_syntax::{
    hir::{self, ClassUnicode, Hir, HirKind, Look},
    utf8::Utf8Sequences,
    Parser,
};

use crate::{
    ast::{byteset_256, byteset_from_range, byteset_set, ExprSet},
    ExprRef,
};

struct StackEntry<'a> {
    ast: &'a Hir,
    args: Vec<ExprRef>,
    result_stack_idx: usize,
    result_vec_offset: usize,
    allow_start: bool,
    allow_end: bool,
}

#[derive(PartialEq, Eq, Debug)]
enum TrieSelector {
    Byte(u8),
    ByteSet(Vec<u32>),
}

struct TrieNode {
    selector: TrieSelector,
    is_match: bool,
    children: Vec<TrieNode>,
}

impl TrieNode {
    fn new(selector: TrieSelector) -> Self {
        Self {
            selector,
            children: Vec::new(),
            is_match: false,
        }
    }

    fn child_at(&mut self, sel: TrieSelector) -> &mut Self {
        let idx = self
            .children
            .iter()
            .position(|child| child.selector == sel)
            .unwrap_or_else(|| {
                let l = self.children.len();
                self.children.push(Self::new(sel));
                l
            });
        &mut self.children[idx]
    }

    fn build_tail(&self, set: &mut ExprSet) -> ExprRef {
        let mut children = Vec::new();
        for child in &self.children {
            children.push(child.build(set));
        }
        if self.is_match {
            children.push(ExprRef::EMPTY_STRING);
        }
        if children.len() == 1 {
            children[0]
        } else {
            set.mk_or(&mut children)
        }
    }

    fn build(&self, set: &mut ExprSet) -> ExprRef {
        let tail = self.build_tail(set);
        let head = match &self.selector {
            TrieSelector::Byte(b) => set.mk_byte(*b),
            TrieSelector::ByteSet(bs) => set.mk_byte_set(bs),
        };
        set.mk_concat(head, tail)
    }
}

impl ExprSet {
    fn handle_unicode_ranges(&mut self, u: &ClassUnicode) -> ExprRef {
        let mut root = TrieNode::new(TrieSelector::Byte(0));

        let key = u
            .ranges()
            .iter()
            .map(|r| (r.start(), r.end()))
            .collect::<Vec<_>>();

        if let Some(r) = self.unicode_cache.get(&key) {
            return *r;
        }

        let ranges = u.ranges();

        for range in ranges {
            for seq in Utf8Sequences::new(range.start(), range.end()) {
                let mut node_ptr = &mut root;
                for s in &seq {
                    let sel = if s.start == s.end {
                        TrieSelector::Byte(s.start)
                    } else {
                        TrieSelector::ByteSet(byteset_from_range(s.start, s.end))
                    };
                    node_ptr = node_ptr.child_at(sel);
                }
                node_ptr.is_match = true;
            }
        }

        let opt = self.optimize;
        self.optimize = false;
        let r = root.build_tail(self);
        self.optimize = opt;

        if !self.any_unicode.is_valid()
            && ranges.len() == 1
            && ranges[0].start() == char::MIN
            && ranges[0].end() == char::MAX
        {
            self.any_unicode = r;
        }

        if !self.any_unicode_non_nl.is_valid()
            && ranges.len() == 2
            && ranges[0].start() == char::MIN
            && ranges[0].end() == (b'\n' - 1) as char
            && ranges[1].start() == (b'\n' + 1) as char
            && ranges[1].end() == char::MAX
        {
            self.any_unicode_non_nl = r;
        }

        self.unicode_cache.insert(key, r);

        r
    }

    fn mk_from_ast(&mut self, ast: &Hir) -> Result<ExprRef> {
        let mut todo = vec![StackEntry {
            ast,
            args: Vec::new(),
            result_stack_idx: 0,
            result_vec_offset: 0,
            allow_start: true,
            allow_end: true,
        }];
        while let Some(mut node) = todo.pop() {
            let subs = node.ast.kind().subs();
            if subs.len() != node.args.len() {
                assert!(node.args.is_empty());
                node.args = subs.iter().map(|_| ExprRef::INVALID).collect();
                let result_stack_idx = todo.len();
                let is_concat = matches!(node.ast.kind(), HirKind::Concat(_));
                let derives_start = matches!(
                    node.ast.kind(),
                    HirKind::Alternation(_) | HirKind::Capture(_)
                );
                let allow_start = (derives_start || is_concat) && node.allow_start;
                let allow_end = (derives_start || is_concat) && node.allow_end;
                todo.push(node);
                for (idx, sub) in subs.iter().enumerate() {
                    todo.push(StackEntry {
                        ast: sub,
                        args: Vec::new(),
                        result_stack_idx,
                        result_vec_offset: idx,
                        allow_start: (!is_concat || idx == 0) && allow_start,
                        allow_end: (!is_concat || idx == subs.len() - 1) && allow_end,
                    });
                }
                continue;
            } else {
                assert!(node.args.iter().all(|&x| x != ExprRef::INVALID));
            }

            let r = match node.ast.kind() {
                HirKind::Empty => ExprRef::EMPTY_STRING,
                HirKind::Literal(bytes) => self.mk_byte_literal(&bytes.0),
                HirKind::Class(hir::Class::Bytes(ranges)) => {
                    let mut bs = byteset_256();
                    for r in ranges.ranges() {
                        for idx in r.start()..=r.end() {
                            byteset_set(&mut bs, idx as usize);
                        }
                    }
                    self.mk_byte_set(&bs)
                }
                HirKind::Class(hir::Class::Unicode(u)) => self.handle_unicode_ranges(u),
                // ignore ^ and $ anchors:
                HirKind::Look(Look::Start) if node.allow_start => ExprRef::EMPTY_STRING,
                HirKind::Look(Look::End) if node.allow_end => ExprRef::EMPTY_STRING,
                HirKind::Look(l) => {
                    bail!("lookarounds not supported yet; {:?}", l)
                }
                HirKind::Repetition(r) => {
                    assert!(node.args.len() == 1);
                    // ignoring greedy flag
                    self.mk_repeat(node.args[0], r.min, r.max.unwrap_or(u32::MAX))
                }
                HirKind::Capture(c) => {
                    assert!(node.args.len() == 1);
                    // use (?P<stop>R) as syntax for lookahead
                    if c.name.as_deref() == Some("stop") {
                        self.mk_lookahead(node.args[0], 0)
                    } else {
                        // ignore capture idx/name
                        node.args[0]
                    }
                }
                HirKind::Concat(args) => {
                    assert!(args.len() == node.args.len());
                    self.mk_concat_vec(&node.args)
                }
                HirKind::Alternation(args) => {
                    assert!(args.len() == node.args.len());
                    self.mk_or(&mut node.args)
                }
            };

            if todo.is_empty() {
                return Ok(r);
            }

            todo[node.result_stack_idx].args[node.result_vec_offset] = r;
        }
        unreachable!()
    }

    pub fn parse_expr(&mut self, mut parser: Parser, rx: &str) -> Result<ExprRef> {
        let hir = parser.parse(rx)?;
        self.mk_from_ast(&hir)
            .map_err(|e| anyhow::anyhow!("{} in regex {:?}", e, rx))
    }
}
