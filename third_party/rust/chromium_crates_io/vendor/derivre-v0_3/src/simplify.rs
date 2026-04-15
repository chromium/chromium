use crate::ast::{
    byteset_clear, byteset_contains, byteset_intersection, byteset_set, byteset_union, Expr,
    ExprFlags, ExprRef, ExprSet, ExprTag,
};

impl ExprSet {
    pub(crate) fn pay(&mut self, cost: usize) {
        self.cost += cost as u64;
    }

    pub fn byte_set_from_byte(&self, b: u8) -> Vec<u32> {
        let mut r = vec![0; self.alphabet_words];
        byteset_set(&mut r, b as usize);
        r
    }

    pub fn mk_byte(&mut self, b: u8) -> ExprRef {
        self.pay(1);
        self.mk(Expr::Byte(b))
    }

    pub fn mk_byte_set(&mut self, s: &[u32]) -> ExprRef {
        assert!(s.len() == self.alphabet_words);
        self.pay(self.alphabet_words);
        let mut num_set = 0;
        for x in s.iter() {
            num_set += x.count_ones();
        }
        if num_set == 0 {
            ExprRef::NO_MATCH
        } else if num_set == 1 {
            for i in 0..self.alphabet_size {
                if byteset_contains(s, i) {
                    return self.mk_byte(i as u8);
                }
            }
            unreachable!()
        } else {
            self.mk(Expr::ByteSet(s))
        }
    }

    pub fn mk_repeat(&mut self, e: ExprRef, min: u32, max: u32) -> ExprRef {
        self.pay(2);
        if e == ExprRef::NO_MATCH {
            if min == 0 {
                ExprRef::EMPTY_STRING
            } else {
                ExprRef::NO_MATCH
            }
        } else if e == ExprRef::EMPTY_STRING {
            ExprRef::EMPTY_STRING
        } else if min > max {
            panic!();
            // ExprRef::NO_MATCH
        } else if max == 0 {
            ExprRef::EMPTY_STRING
        } else if min == 1 && max == 1 {
            e
        } else {
            let e_flags = self.get_flags(e);
            let min = if e_flags.is_nullable() { 0 } else { min };
            let flags = ExprFlags::from_nullable_positive(min == 0, e_flags.is_positive());
            self.mk(Expr::Repeat(flags, e, min, max))
        }
    }

    fn flatten_tag(&self, exp_tag: ExprTag, args: &mut Vec<ExprRef>) {
        let mut i = 0;
        while i < args.len() {
            let tag = self.get_tag(args[i]);
            if tag == exp_tag {
                // ok, we found tag, we can no longer work only with the original vector
                let the_rest = args[i..].to_vec();
                args.truncate(i);
                for a in the_rest {
                    let tag = self.get_tag(a);
                    if tag != exp_tag {
                        args.push(a);
                    } else {
                        args.extend_from_slice(self.get_args(a));
                    }
                    i += 1;
                }
                return;
            }
            i += 1;
        }
    }

    // Complexity of mk_X(args) is O(n log n) where n = |flatten(X, args)|

    pub fn mk_or(&mut self, args: &mut Vec<ExprRef>) -> ExprRef {
        // TODO deal with byte ranges
        self.flatten_tag(ExprTag::Or, args);
        self.pay(2 * args.len());
        args.sort_unstable();
        let mut dp = 0;
        let mut prev = ExprRef::NO_MATCH;
        let mut nullable = false;
        let mut num_bytes = 0;
        let mut num_lookahead = 0;
        let mut positive = false;
        for idx in 0..args.len() {
            let arg = args[idx];
            if arg == prev || arg == ExprRef::NO_MATCH {
                continue;
            }
            if arg == ExprRef::ANY_BYTE_STRING {
                return ExprRef::ANY_BYTE_STRING;
            }
            match self.get(arg) {
                Expr::Byte(_) | Expr::ByteSet(_) => {
                    num_bytes += 1;
                }
                Expr::Lookahead(_, _, _) => {
                    num_lookahead += 1;
                }
                _ => {}
            }
            let f = self.get_flags(arg);
            if !nullable && f.is_nullable() {
                nullable = true;
            }
            if !positive && f.is_positive() {
                positive = true;
            }
            args[dp] = arg;
            dp += 1;
            prev = arg;
        }
        args.truncate(dp);

        // TODO we should probably do sth similar in And
        if num_bytes > 1 {
            let mut byteset = vec![0u32; self.alphabet_words];
            self.pay(args.len());
            args.retain(|&e| {
                let n = self.get(e);
                match n {
                    Expr::Byte(b) => {
                        byteset_set(&mut byteset, b as usize);
                        false
                    }
                    Expr::ByteSet(s) => {
                        byteset_union(&mut byteset, s);
                        false
                    }
                    _ => true,
                }
            });
            let node = self.mk_byte_set(&byteset);
            add_to_sorted(args, node);
        }

        if num_lookahead > 1 {
            let mut lookahead = vec![];
            self.pay(args.len());
            args.retain(|&e| {
                let n = self.get(e);
                match n {
                    Expr::Lookahead(_, inner, n) => {
                        lookahead.push((e, inner, n));
                        false
                    }
                    _ => true,
                }
            });
            lookahead.sort_by_key(|&(_, e, n)| (e.0, n));

            let mut prev = ExprRef::INVALID;
            for ll in lookahead.iter() {
                let (l, inner, _) = *ll;
                if inner == prev {
                    continue;
                }
                prev = inner;
                args.push(l);
            }

            args.sort_unstable();
        }

        if args.is_empty() {
            ExprRef::NO_MATCH
        } else if args.len() == 1 {
            args[0]
        } else {
            let flags = ExprFlags::from_nullable_positive(nullable, positive);
            if self.optimize {
                self.or_optimized(flags, args)
            } else {
                self.mk(Expr::Or(flags, args))
            }
        }
    }

    fn or_optimized(&mut self, flags: ExprFlags, args: &mut [ExprRef]) -> ExprRef {
        let args0 = args.to_vec();

        args.sort_unstable_by(|&a, &b| self.iter_concat_bytes(a).cmp(self.iter_concat_bytes(b)));

        let mut prev = None;
        let mut has_double = false;
        for c in args.iter() {
            let c0 = self.iter_concat_bytes(*c).next();
            if c0 == prev {
                has_double = true;
                break;
            }
            prev = c0;
        }
        if !has_double {
            self.mk(Expr::Or(flags, &args0))
        } else {
            self.optimize = false;
            let mut args = args
                .iter()
                .map(|a| ConcatBytePointer::new(*a))
                .collect::<Vec<_>>();
            let r = self.trie_rec(args.as_mut_slice(), 0);
            self.optimize = true;
            r
        }
    }

    pub fn mk_prefix_tree(&mut self, mut branches: Vec<(Vec<u8>, ExprRef)>) -> ExprRef {
        branches.sort_unstable_by(|a, b| a.0.cmp(&b.0));

        let mut prev = None;
        let mut has_double = false;
        for c in branches.iter() {
            let c0 = c.0.first();
            if c0 == prev {
                has_double = true;
                break;
            }
            prev = c0;
        }

        let prev_opt = self.optimize;
        self.optimize = false;

        let r = if !has_double {
            let mut refs = branches
                .iter()
                .map(|(p, e)| self.mk_byte_concat(p, *e))
                .collect::<Vec<_>>();
            self.mk_or(&mut refs)
        } else {
            let mut args = branches
                .into_iter()
                .map(|a| ConcatBytePointer {
                    pending: a.0,
                    pending_ptr: 0,
                    current: Some(a.1),
                })
                .collect::<Vec<_>>();
            self.trie_rec(args.as_mut_slice(), 0)
        };

        self.optimize = prev_opt;

        r
    }

    // The idea is to optimize regexps like identifier1|identifier2|...|identifier50000
    // into a "trie" with shared prefixes;
    // for example: (foo|far|bar|baz) => (ba[rz]|f(oo|ar))
    fn trie_rec(&mut self, args: &mut [ConcatBytePointer], depth: usize) -> ExprRef {
        if args.len() == 1 {
            return args[0].snapshot(self);
        }

        // limit recursion depth
        if depth > 100 {
            let mut args = args.iter().map(|a| a.snapshot(self)).collect::<Vec<_>>();
            return self.mk_or(&mut args);
        }

        let mut common = vec![];
        let last_idx = args.len() - 1;
        loop {
            let a_0 = args[0].clone();
            let a_end = args[last_idx].clone();
            let a = args[0].next(self);
            let b = args[last_idx].next(self);
            if a != b {
                args[0] = a_0;
                args[last_idx] = a_end;
                break;
            }
            let a = a.unwrap();
            let b = b.unwrap();

            a.push_owned_to(&mut common);

            // assert!(a != ExprRef::EMPTY_STRING);
            for arg in &mut args[1..last_idx] {
                let a = arg.next(self).unwrap();
                assert!(a == b);
            }
        }
        assert!(depth == 0 || !common.is_empty());

        let mut idx = 0;

        let mut alternatives = vec![];
        while idx < args.len() {
            let cur = args[idx].peek(self);
            let mut next = idx + 1;
            while next < args.len() && args[next].peek(self) == cur {
                next += 1;
            }

            if cur.is_some() {
                alternatives.push(self.trie_rec(&mut args[idx..next], depth + 1));
            } else {
                alternatives.push(ExprRef::EMPTY_STRING);
            }

            idx = next;
        }

        let alts = self.mk_or(&mut alternatives);
        common.push(OwnedConcatElement::Expr(alts));
        self._mk_concat_vec(common)
    }

    pub fn mk_byte_set_not(&mut self, x: ExprRef) -> ExprRef {
        match self.get(x) {
            Expr::Byte(b) => {
                let mut r = vec![!0u32; self.alphabet_words];
                byteset_clear(&mut r, b as usize);
                self.mk_byte_set(&r)
            }
            Expr::ByteSet(bs) => self.mk_byte_set(&bs.iter().map(|v| !*v).collect::<Vec<_>>()),
            _ => panic!(),
        }
    }

    pub fn mk_byte_set_or(&mut self, args: &[ExprRef]) -> ExprRef {
        self.mk_byte_set_or_core(args, false)
    }

    pub fn mk_byte_set_neg_or(&mut self, args: &[ExprRef]) -> ExprRef {
        self.mk_byte_set_or_core(args, true)
    }

    fn mk_byte_set_or_core(&mut self, args: &[ExprRef], neg: bool) -> ExprRef {
        let mut byteset = vec![0u32; self.alphabet_words];
        for e in args {
            let n = self.get(*e);
            match n {
                Expr::Byte(b) => {
                    byteset_set(&mut byteset, b as usize);
                }
                Expr::ByteSet(s) => {
                    byteset_union(&mut byteset, s);
                }
                _ => panic!(),
            }
        }
        if neg {
            byteset = byteset.iter().map(|v| !*v).collect();
            for idx in self.alphabet_size..self.alphabet_words * 32 {
                byteset_clear(&mut byteset, idx);
            }
        }
        self.mk_byte_set(&byteset)
    }

    pub fn mk_byte_set_and(&mut self, aa: ExprRef, bb: ExprRef) -> ExprRef {
        if aa == bb {
            aa
        } else {
            match (self.get(aa), self.get(bb)) {
                (Expr::Byte(_), Expr::Byte(_)) => ExprRef::NO_MATCH,
                (Expr::Byte(a), Expr::ByteSet(b)) => {
                    if byteset_contains(b, a as usize) {
                        aa
                    } else {
                        ExprRef::NO_MATCH
                    }
                }
                (Expr::ByteSet(a), Expr::Byte(b)) => {
                    if byteset_contains(a, b as usize) {
                        bb
                    } else {
                        ExprRef::NO_MATCH
                    }
                }
                (Expr::ByteSet(a), Expr::ByteSet(b)) => {
                    let mut a = a.to_vec();
                    byteset_intersection(&mut a, b);
                    self.mk_byte_set(&a)
                }
                _ => panic!(),
            }
        }
    }

    pub fn mk_byte_set_sub(&mut self, aa: ExprRef, bb: ExprRef) -> ExprRef {
        match (self.get(aa), self.get(bb)) {
            (Expr::Byte(x), Expr::Byte(y)) => {
                if x == y {
                    ExprRef::NO_MATCH
                } else {
                    aa
                }
            }
            (Expr::Byte(a), Expr::ByteSet(b)) => {
                if byteset_contains(b, a as usize) {
                    ExprRef::NO_MATCH
                } else {
                    aa
                }
            }
            (Expr::ByteSet(a), Expr::Byte(b)) => {
                if byteset_contains(a, b as usize) {
                    let mut a = a.to_vec();
                    byteset_clear(&mut a, b as usize);
                    self.mk_byte_set(&a)
                } else {
                    aa
                }
            }
            (Expr::ByteSet(a), Expr::ByteSet(b)) => {
                let mut a = a.to_vec();
                let b = b.iter().map(|v| !*v).collect::<Vec<_>>();
                byteset_intersection(&mut a, &b);
                self.mk_byte_set(&a)
            }
            _ => panic!(),
        }
    }

    pub fn mk_remainder_is(
        &mut self,
        divisor: u32,
        remainder: u32,
        scale: u32,
        fractional_part: bool,
    ) -> ExprRef {
        assert!(divisor > 0);
        assert!(remainder <= divisor);
        self.pay(1);
        if !fractional_part {
            self.mk(Expr::RemainderIs {
                divisor,
                remainder,
                scale,
                fractional_part,
            })
        } else {
            if scale == 0 && remainder == 0 {
                // We're done
                return ExprRef::EMPTY_STRING;
            }
            let scale_multiplier = 10u32.pow(scale);
            let remainder_to_go = (divisor - remainder) % divisor;
            if remainder_to_go < scale_multiplier {
                if scale_multiplier <= divisor {
                    // If our scale has shrunken smaller than our divisor, we can force the rest
                    // of the digits
                    let forced_digits =
                        format!("{:0>width$}", remainder_to_go, width = scale as usize);
                    // TODO: trim trailing zeros?
                    let mapped = forced_digits
                        .as_bytes()
                        .iter()
                        .map(|b| self.digits[(b - b'0') as usize])
                        .collect::<Vec<_>>();
                    self.mk_byte_literal(&mapped)
                } else {
                    self.mk(Expr::RemainderIs {
                        divisor,
                        remainder,
                        scale,
                        fractional_part,
                    })
                }
            } else {
                ExprRef::NO_MATCH
            }
        }
    }

    // this avoids allocation when hitting the hash-cons
    pub(crate) fn mk_and2(&mut self, a: ExprRef, b: ExprRef) -> ExprRef {
        self.pay(2);
        let (a, b) = if a < b { (a, b) } else { (b, a) };
        let nullable = self.is_nullable(a) && self.is_nullable(b);
        let flags = ExprFlags::from_nullable_positive(nullable, nullable);
        self.mk(Expr::And(flags, &[a, b]))
    }

    pub fn mk_and(&mut self, args: &mut Vec<ExprRef>) -> ExprRef {
        self.flatten_tag(ExprTag::And, args);
        self.pay(2 * args.len());
        args.sort_unstable();
        let mut dp = 0;
        let mut prev = ExprRef::ANY_BYTE_STRING;
        let mut had_empty = false;
        let mut nullable = true;
        for idx in 0..args.len() {
            let arg = args[idx];
            if arg == prev || arg == ExprRef::ANY_BYTE_STRING {
                continue;
            }
            if arg == ExprRef::NO_MATCH {
                return ExprRef::NO_MATCH;
            }
            if arg == ExprRef::EMPTY_STRING {
                had_empty = true;
            }
            if nullable && !self.is_nullable(arg) {
                nullable = false;
            }
            args[dp] = arg;
            dp += 1;
            prev = arg;
        }
        args.truncate(dp);

        if args.is_empty() {
            ExprRef::ANY_BYTE_STRING
        } else if args.len() == 1 {
            args[0]
        } else if had_empty {
            if nullable {
                ExprRef::EMPTY_STRING
            } else {
                ExprRef::NO_MATCH
            }
        } else {
            let positive = nullable; // if all branches are nullable, then it's also positive
            let flags = ExprFlags::from_nullable_positive(nullable, positive);
            self.mk(Expr::And(flags, args))
        }
    }

    pub fn iter_concat(&self, root: ExprRef) -> ConcatIter {
        ConcatIter {
            exprs: self,
            current: Some(root),
        }
    }

    pub fn iter_concat_bytes(&self, root: ExprRef) -> ConcatByteIter {
        ConcatByteIter {
            exprs: self,
            pointer: ConcatBytePointer::new(root),
        }
    }

    fn is_concat(&self, e: ExprRef) -> bool {
        let tag = self.get_tag(e);
        tag == ExprTag::Concat || tag == ExprTag::ByteConcat
    }

    pub fn mk_concat_vec(&mut self, args: &[ExprRef]) -> ExprRef {
        let mut expanded_args = Vec::with_capacity(args.len());
        for idx in 0..args.len() {
            let arg = args[idx];
            if idx == args.len() - 1 {
                if arg == ExprRef::NO_MATCH {
                    return ExprRef::NO_MATCH;
                } else if arg != ExprRef::EMPTY_STRING {
                    expanded_args.push(OwnedConcatElement::Expr(arg));
                }
            } else {
                // flatten everything except for the last element
                for a in self.iter_concat(arg) {
                    if !a.push_owned_to(&mut expanded_args) {
                        return ExprRef::NO_MATCH;
                    }
                }
            }
        }

        self._mk_concat_vec(expanded_args)
    }

    pub(crate) fn _mk_concat_vec(&mut self, args: Vec<OwnedConcatElement>) -> ExprRef {
        let len = args.len();
        if len == 0 {
            ExprRef::EMPTY_STRING
        } else {
            let mut r = match &args[len - 1] {
                OwnedConcatElement::Expr(e) => *e,
                OwnedConcatElement::Bytes(b) => self.mk_byte_literal(b),
            };

            for arg in args[..len - 1].iter().rev() {
                match arg {
                    OwnedConcatElement::Expr(e) => {
                        r = self.mk_concat(*e, r);
                    }
                    OwnedConcatElement::Bytes(b) => {
                        r = self.mk_byte_concat(b, r);
                    }
                }
            }

            r
        }
    }

    pub fn mk_concat(&mut self, a: ExprRef, b: ExprRef) -> ExprRef {
        self.pay(2);
        if a == ExprRef::EMPTY_STRING {
            return b;
        }
        if b == ExprRef::EMPTY_STRING {
            return a;
        }
        if a == ExprRef::NO_MATCH || b == ExprRef::NO_MATCH {
            return ExprRef::NO_MATCH;
        }

        if self.is_concat(a) {
            let mut expanded_args = vec![];
            for e in self.iter_concat(a) {
                if !e.push_owned_to(&mut expanded_args) {
                    return ExprRef::NO_MATCH;
                }
            }
            expanded_args.push(OwnedConcatElement::Expr(b));
            return self._mk_concat_vec(expanded_args);
        }

        let fa = self.get_flags(a);
        let fb = self.get_flags(b);

        let nullable = fa.is_nullable() && fb.is_nullable();
        let positive = fa.is_positive() && fb.is_positive();
        let flags = ExprFlags::from_nullable_positive(nullable, positive);
        self.mk(Expr::Concat(flags, [a, b]))
    }

    pub fn mk_byte_concat(&mut self, mut s: &[u8], mut tail: ExprRef) -> ExprRef {
        if s.is_empty() {
            return tail;
        }
        if s.len() == 1 && tail == ExprRef::EMPTY_STRING {
            return self.mk_byte(s[0]);
        }
        self.pay(2 + s.len() / ExprRef::MAX_BYTE_CONCAT);
        let flags = ExprFlags::from_nullable_positive(false, self.is_positive(tail));
        loop {
            if s.len() <= ExprRef::MAX_BYTE_CONCAT {
                return self.mk(Expr::ByteConcat(flags, s, tail));
            } else {
                let idx = s.len() - ExprRef::MAX_BYTE_CONCAT;
                tail = self.mk(Expr::ByteConcat(flags, &s[idx..], tail));
                s = &s[..idx];
            }
        }
    }

    pub fn mk_byte_literal(&mut self, s: &[u8]) -> ExprRef {
        self.mk_byte_concat(s, ExprRef::EMPTY_STRING)
    }

    pub fn mk_literal(&mut self, s: &str) -> ExprRef {
        self.mk_byte_literal(s.as_bytes())
    }

    pub fn mk_not(&mut self, e: ExprRef) -> ExprRef {
        self.pay(2);
        if e == ExprRef::EMPTY_STRING {
            ExprRef::NON_EMPTY_BYTE_STRING
        } else if e == ExprRef::NON_EMPTY_BYTE_STRING {
            ExprRef::EMPTY_STRING
        } else if e == ExprRef::ANY_BYTE_STRING {
            ExprRef::NO_MATCH
        } else if e == ExprRef::NO_MATCH {
            ExprRef::ANY_BYTE_STRING
        } else {
            let n = self.get(e);
            if let Expr::Not(_, e2) = n {
                return e2;
            }
            let nullable_positive = !n.nullable();
            let flags = ExprFlags::from_nullable_positive(nullable_positive, nullable_positive);
            self.mk(Expr::Not(flags, e))
        }
    }

    pub fn mk_lookahead(&mut self, mut e: ExprRef, offset: u32) -> ExprRef {
        self.pay(2);
        if e == ExprRef::NO_MATCH {
            return ExprRef::NO_MATCH;
        }

        let flags = self.get_flags(e);
        if flags.is_nullable() {
            e = ExprRef::EMPTY_STRING;
        }
        self.mk(Expr::Lookahead(flags, e, offset))
    }
}

fn add_to_sorted(args: &mut Vec<ExprRef>, e: ExprRef) {
    let idx = args.binary_search(&e).unwrap_or_else(|x| x);
    assert!(idx == args.len() || args[idx] != e);
    args.insert(idx, e);
}

pub enum ConcatElement<'a> {
    Expr(ExprRef),
    Bytes(&'a [u8]),
}

impl ConcatElement<'_> {
    pub fn push_owned_to(&self, out: &mut Vec<OwnedConcatElement>) -> bool {
        match self {
            ConcatElement::Bytes(bb) => match out.last_mut() {
                Some(OwnedConcatElement::Bytes(ref mut exp)) => {
                    exp.extend_from_slice(bb);
                }
                _ => {
                    out.push(OwnedConcatElement::Bytes(bb.to_vec()));
                }
            },
            ConcatElement::Expr(e) => {
                if *e == ExprRef::NO_MATCH {
                    return false;
                }
                if *e != ExprRef::EMPTY_STRING {
                    out.push(OwnedConcatElement::Expr(*e));
                }
            }
        }
        true
    }
}

pub enum OwnedConcatElement {
    Expr(ExprRef),
    Bytes(Vec<u8>),
}

#[derive(PartialEq, Eq, Debug, PartialOrd, Ord)]
pub enum ByteConcatElement {
    Byte(u8),
    Expr(ExprRef),
}

impl ByteConcatElement {
    pub fn push_owned_to(&self, out: &mut Vec<OwnedConcatElement>) {
        match self {
            ByteConcatElement::Byte(b) => match out.last_mut() {
                Some(OwnedConcatElement::Bytes(ref mut exp)) => {
                    exp.push(*b);
                }
                _ => {
                    out.push(OwnedConcatElement::Bytes(vec![*b]));
                }
            },
            ByteConcatElement::Expr(e) => {
                if *e == ExprRef::NO_MATCH {
                    panic!();
                }
                if *e != ExprRef::EMPTY_STRING {
                    out.push(OwnedConcatElement::Expr(*e));
                }
            }
        }
    }
}

pub struct ConcatIter<'a> {
    exprs: &'a ExprSet,
    current: Option<ExprRef>,
}

pub struct ConcatByteIter<'a> {
    exprs: &'a ExprSet,
    pointer: ConcatBytePointer,
}

impl Iterator for ConcatByteIter<'_> {
    type Item = ByteConcatElement;

    fn next(&mut self) -> Option<Self::Item> {
        self.pointer.next(self.exprs)
    }
}

#[derive(Clone)]
struct ConcatBytePointer {
    pending_ptr: usize,
    pending: Vec<u8>,
    current: Option<ExprRef>,
}

impl ConcatBytePointer {
    pub fn new(curr: ExprRef) -> Self {
        ConcatBytePointer {
            pending_ptr: 0,
            pending: Vec::new(),
            current: Some(curr),
        }
    }

    pub fn peek(&self, exprset: &ExprSet) -> Option<ByteConcatElement> {
        let mut copy = self.clone();
        copy.next(exprset)
    }

    pub fn next(&mut self, exprset: &ExprSet) -> Option<ByteConcatElement> {
        if self.pending_ptr < self.pending.len() {
            let b = self.pending[self.pending_ptr];
            self.pending_ptr += 1;
            return Some(ByteConcatElement::Byte(b));
        }

        let curr = self.current?;

        let mut it = exprset.iter_concat(curr);
        let tmp = it.next();
        self.current = it.current;
        match tmp {
            Some(ConcatElement::Bytes(bytes)) => {
                let b0 = bytes[0];
                self.pending = bytes[1..].to_vec();
                self.pending_ptr = 0;
                Some(ByteConcatElement::Byte(b0))
            }
            Some(ConcatElement::Expr(expr)) => Some(ByteConcatElement::Expr(expr)),
            None => None,
        }
    }

    pub fn snapshot(&self, exprset: &mut ExprSet) -> ExprRef {
        let tail = self.current.unwrap_or(ExprRef::EMPTY_STRING);
        if self.pending_ptr >= self.pending.len() {
            tail
        } else {
            exprset.mk_byte_concat(&self.pending[self.pending_ptr..], tail)
        }
    }
}

impl<'a> Iterator for ConcatIter<'a> {
    type Item = ConcatElement<'a>;

    fn next(&mut self) -> Option<Self::Item> {
        let curr = self.current?;
        let expr = self.exprs.get(curr);
        match expr {
            Expr::Concat(_, [l, r]) => {
                self.current = Some(r);
                if let Some(bytes) = self.exprs.get_bytes(l) {
                    Some(ConcatElement::Bytes(bytes))
                } else {
                    Some(ConcatElement::Expr(l))
                }
            }
            Expr::ByteConcat(_, bytes, tail) => {
                self.current = Some(tail);
                Some(ConcatElement::Bytes(bytes))
            }
            _ => {
                self.current = None;
                if let Some(bytes) = self.exprs.get_bytes(curr) {
                    Some(ConcatElement::Bytes(bytes))
                } else {
                    Some(ConcatElement::Expr(curr))
                }
            }
        }
    }
}
