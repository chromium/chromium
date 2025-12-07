use anyhow::{bail, Result};
use regex_syntax::{
    hir::{self, ClassUnicode, Hir, HirKind, Look},
    utf8::Utf8Sequences,
    Parser,
};

use crate::{
    ast::{byteset_256, byteset_from_range, byteset_set, ExprSet},
    regexbuilder::write_regex,
    ExprRef,
};

struct StackEntry<'a> {
    ast: &'a Hir,
    args: Vec<ExprRef>,
    anchored: Vec<(bool, bool)>,
    result_stack_idx: usize,
    result_vec_offset: usize,
    at_start: bool,
    at_end: bool,
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
            && ranges[0].start() == '\0'
            && ranges[0].end() == char::MAX
        {
            self.any_unicode = r;
        }

        if !self.any_unicode_non_nl.is_valid()
            && ranges.len() == 2
            && ranges[0].start() == '\0'
            && ranges[0].end() == (b'\n' - 1) as char
            && ranges[1].start() == (b'\n' + 1) as char
            && ranges[1].end() == char::MAX
        {
            self.any_unicode_non_nl = r;
        }

        self.unicode_cache.insert(key, r);

        r
    }

    fn mk_any_unicode_star(&mut self) -> ExprRef {
        if self.any_unicode_star.is_valid() {
            return self.any_unicode_star;
        }
        let mut all = ClassUnicode::empty();
        all.negate();
        let any_unicode = self.handle_unicode_ranges(&all);
        assert_eq!(any_unicode, self.any_unicode);
        self.any_unicode_star = self.mk_repeat(any_unicode, 0, u32::MAX);
        self.any_unicode_star
    }

    fn mk_from_ast(&mut self, ast: &Hir, for_search: bool) -> Result<ExprRef> {
        let mut todo = vec![StackEntry {
            ast,
            args: Vec::new(),
            anchored: Vec::new(),
            result_stack_idx: 0,
            result_vec_offset: 0,
            at_start: true,
            at_end: true,
        }];
        while let Some(mut node) = todo.pop() {
            let subs = node.ast.kind().subs();
            if subs.len() != node.args.len() {
                assert!(node.args.is_empty());
                let n_args = subs.len();
                node.args = vec![ExprRef::INVALID; n_args];
                if for_search {
                    node.anchored = vec![(false, false); n_args];
                }
                let result_stack_idx = todo.len();
                let is_concat = matches!(node.ast.kind(), HirKind::Concat(_));
                let derives_start = matches!(
                    node.ast.kind(),
                    HirKind::Alternation(_) | HirKind::Capture(_)
                );
                let at_start = (derives_start || is_concat) && node.at_start;
                let at_end = (derives_start || is_concat) && node.at_end;
                todo.push(node);
                for (idx, sub) in subs.iter().enumerate() {
                    todo.push(StackEntry {
                        ast: sub,
                        args: Vec::new(),
                        anchored: Vec::new(),
                        result_stack_idx,
                        result_vec_offset: idx,
                        at_start: (!is_concat || idx == 0) && at_start,
                        at_end: (!is_concat || idx == subs.len() - 1) && at_end,
                    });
                }
                continue;
            } else {
                assert!(node.args.iter().all(|&x| x != ExprRef::INVALID));
            }

            let mut anchored_start = false;
            let mut anchored_end = false;

            let mut r = match node.ast.kind() {
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
                HirKind::Look(Look::Start) if node.at_start => {
                    anchored_start = true;
                    ExprRef::EMPTY_STRING
                }
                HirKind::Look(Look::End) if node.at_end => {
                    anchored_end = true;
                    ExprRef::EMPTY_STRING
                }
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
                    if for_search {
                        (anchored_start, anchored_end) = node.anchored[0];
                    }
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
                    if for_search {
                        anchored_start = node.anchored[0].0;
                        anchored_end = node.anchored[node.args.len() - 1].1;
                    }
                    self.mk_concat_vec(&node.args)
                }
                HirKind::Alternation(args) => {
                    assert!(args.len() == node.args.len());
                    if for_search && (node.at_start || node.at_end) {
                        let mut all_start = true;
                        let mut all_end = true;
                        let mut some_start = false;
                        let mut some_end = false;
                        for (st, en) in node.anchored.iter() {
                            all_start &= st;
                            all_end &= en;
                            some_start |= st;
                            some_end |= en;
                        }
                        if some_start || some_end {
                            anchored_start = some_start;
                            anchored_end = some_end;
                            if !all_start || !all_end {
                                let dot_star = self.mk_any_unicode_star();
                                for ((st, en), arg) in
                                    node.anchored.iter().zip(node.args.iter_mut())
                                {
                                    let needs_st = !*st && anchored_start;
                                    let needs_en = !*en && anchored_end;
                                    if needs_en {
                                        *arg = self.mk_concat(*arg, dot_star);
                                    }
                                    if needs_st {
                                        *arg = self.mk_concat(dot_star, *arg);
                                    }
                                }
                            }
                        }
                    }
                    self.mk_or(&mut node.args)
                }
            };

            if todo.is_empty() {
                if for_search {
                    let dot_star = self.mk_any_unicode_star();
                    if !anchored_end {
                        r = self.mk_concat(r, dot_star);
                    }
                    if !anchored_start {
                        r = self.mk_concat(dot_star, r);
                    }
                }
                return Ok(r);
            }

            todo[node.result_stack_idx].args[node.result_vec_offset] = r;
            if for_search {
                todo[node.result_stack_idx].anchored[node.result_vec_offset] =
                    (anchored_start, anchored_end);
            }
        }
        unreachable!()
    }

    pub fn parse_expr(
        &mut self,
        mut parser: Parser,
        rx: &str,
        for_search: bool,
    ) -> Result<ExprRef> {
        let hir = parser.parse(rx)?;
        self.mk_from_ast(&hir, for_search).map_err(|e| {
            let mut err = format!("{e} in regex ");
            write_regex(&mut err, rx);
            anyhow::anyhow!(err)
        })
    }
}
