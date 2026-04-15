use std::{
    fmt::Debug,
    hash::Hash,
    ops::{BitOr, RangeInclusive},
};

use crate::HashMap;
use crate::{hashcons::VecHashCons, pp::PrettyPrinter, simplify::OwnedConcatElement};
use bytemuck_derive::{Pod, Zeroable};
use strum::FromRepr;

#[derive(Pod, Zeroable, Clone, Copy, Debug, PartialEq, Eq, Hash, PartialOrd, Ord)]
#[repr(transparent)]
pub struct ExprRef(pub(crate) u32);

impl ExprRef {
    pub const INVALID: ExprRef = ExprRef(0);
    pub const EMPTY_STRING: ExprRef = ExprRef(1);
    pub const NO_MATCH: ExprRef = ExprRef(2);
    // the ones below can match invalid UTF8
    pub const ANY_BYTE: ExprRef = ExprRef(3);
    pub const ANY_BYTE_STRING: ExprRef = ExprRef(4);
    pub const NON_EMPTY_BYTE_STRING: ExprRef = ExprRef(5);

    pub const MAX_BYTE_CONCAT: usize = 32 - 1;

    pub fn new(id: u32) -> Self {
        // assert!(id != 0, "ExprRef(0) is reserved for invalid reference");
        ExprRef(id)
    }

    pub fn is_valid(&self) -> bool {
        self.0 != 0
    }
    pub fn as_usize(&self) -> usize {
        self.0 as usize
    }
    pub fn as_u32(&self) -> u32 {
        self.0
    }
}

pub enum Expr<'a> {
    EmptyString,
    NoMatch,
    Byte(u8),
    ByteSet(&'a [u32]),
    // RemainderIs (with fractional_part=false) matches numbers N where (N + remainder*10^len(N)) % divisor*10^-scale == 0.
    // For remainder = 0, this is equivalent to N being divisible by d*10^-scale.
    // The remainder = divisor case is the same, but we exclude the empty string.
    // fractional_part = true is only for bookkeeping and signifies that we have produced a decimal point.
    RemainderIs {
        divisor: u32,
        remainder: u32,
        scale: u32,
        fractional_part: bool,
    },
    Lookahead(ExprFlags, ExprRef, u32),
    Not(ExprFlags, ExprRef),
    Repeat(ExprFlags, ExprRef, u32, u32),
    Concat(ExprFlags, [ExprRef; 2]),
    Or(ExprFlags, &'a [ExprRef]),
    And(ExprFlags, &'a [ExprRef]),
    // This is equivalent to Concat(Byte(b0), Concat(Byte(b1), ... tail))
    ByteConcat(ExprFlags, &'a [u8], ExprRef),
}

// Note on ByteConcat and binary Concat:
// Testcase here is a large JSON enum (2000 entries) with ~50 bytes per entry.
// Commit https://github.com/microsoft/derivre/commit/38db596028758e7f56db1510003d02ae89070030
// changed Concat from n-ary to binary. This simplifies derivative computation
// (max mask time 14ms -> 7.5ms), but slows down initial regex construction (15ms -> 75ms).
// The specialized ByteConcat improves a little max mask (6.5ms), but more importantly
// speeds up initial construction (11ms).
// Thus the net effect is 2x faster matching on large enums, and slightly faster regex construction.
// This comes at the cost of significant code complexity, so we may need to rethink that.

#[derive(Clone, Copy)]
pub struct ExprFlags(u32);
impl ExprFlags {
    pub const NULLABLE: ExprFlags = ExprFlags(1 << 8);
    pub const POSITIVE: ExprFlags = ExprFlags(1 << 9);
    pub const ZERO: ExprFlags = ExprFlags(0);

    pub const POSITIVE_NULLABLE: ExprFlags =
        ExprFlags(ExprFlags::POSITIVE.0 | ExprFlags::NULLABLE.0);

    pub fn is_nullable(&self) -> bool {
        self.0 & ExprFlags::NULLABLE.0 != 0
    }

    pub fn is_positive(&self) -> bool {
        self.0 & ExprFlags::POSITIVE.0 != 0
    }

    pub fn from_nullable_positive(nullable: bool, positive: bool) -> Self {
        if nullable {
            // anything nullable is also positive
            Self::POSITIVE_NULLABLE
        } else if positive {
            Self::POSITIVE
        } else {
            Self::ZERO
        }
    }

    fn encode(&self, tag: ExprTag) -> u32 {
        self.0 | tag as u32
    }
}

#[derive(Clone, Copy, Debug, PartialEq, Eq, FromRepr)]
#[repr(u8)]
pub enum ExprTag {
    EmptyString = 1,
    NoMatch,
    Byte,
    ByteSet,
    ByteConcat,
    RemainderIs,
    Lookahead,
    Not,
    Repeat,
    Concat,
    Or,
    And,
}

impl ExprTag {
    #[inline(always)]
    fn from_u8(x: u8) -> Self {
        ExprTag::from_repr(x).unwrap()
    }
}

#[inline(always)]
pub fn byteset_contains(s: &[u32], b: usize) -> bool {
    s[b / 32] & (1 << (b % 32)) != 0
}

#[inline(always)]
pub fn byteset_set(s: &mut [u32], b: usize) {
    s[b / 32] |= 1 << (b % 32);
}

#[inline(always)]
pub fn byteset_clear(s: &mut [u32], b: usize) {
    s[b / 32] &= !(1 << (b % 32));
}

#[inline(always)]
pub fn byteset_set_range(s: &mut [u32], range: RangeInclusive<u8>) {
    for elt in range {
        byteset_set(s, elt as usize);
    }
}

#[inline(always)]
pub fn byteset_union(s: &mut [u32], other: &[u32]) {
    for i in 0..s.len() {
        s[i] |= other[i];
    }
}

#[inline(always)]
pub fn byteset_intersection(s: &mut [u32], other: &[u32]) {
    for i in 0..s.len() {
        s[i] &= other[i];
    }
}

pub fn byteset_256() -> Vec<u32> {
    vec![0u32; 256 / 32]
}

pub fn byteset_from_range(start: u8, end: u8) -> Vec<u32> {
    let mut s = byteset_256();
    byteset_set_range(&mut s, start..=end);
    s
}

impl<'a> Expr<'a> {
    pub fn surely_no_match(&self, b: u8) -> bool {
        match self {
            Expr::EmptyString => true,
            Expr::NoMatch => true,
            Expr::Byte(b2) => b != *b2,
            Expr::ByteSet(s) => !byteset_contains(s, b as usize),
            Expr::ByteConcat(_, bs, _) => bs[0] != b,
            _ => false,
        }
    }

    pub fn matches_byte(&self, b: u8) -> bool {
        match self {
            Expr::EmptyString => false,
            Expr::NoMatch => false,
            Expr::Byte(b2) => b == *b2,
            Expr::ByteSet(s) => byteset_contains(s, b as usize),
            Expr::ByteConcat(_, bs, _) => bs[0] == b,
            _ => panic!("not a simple expression"),
        }
    }

    pub fn args(&self) -> &[ExprRef] {
        match self {
            Expr::Concat(_, es) => es,
            Expr::Or(_, es) | Expr::And(_, es) => es,
            Expr::Lookahead(_, e, _)
            | Expr::Not(_, e)
            | Expr::Repeat(_, e, _, _)
            | Expr::ByteConcat(_, _, e) => std::slice::from_ref(e),
            Expr::RemainderIs { .. }
            | Expr::EmptyString
            | Expr::NoMatch
            | Expr::Byte(_)
            | Expr::ByteSet(_) => &[],
        }
    }

    #[inline]
    fn get_flags(&self) -> ExprFlags {
        match self {
            Expr::EmptyString => ExprFlags::POSITIVE_NULLABLE,
            Expr::RemainderIs { remainder, .. } => {
                if *remainder == 0 {
                    ExprFlags::POSITIVE_NULLABLE
                } else {
                    ExprFlags::POSITIVE
                }
            }
            Expr::NoMatch => ExprFlags::ZERO,
            Expr::Byte(_) | Expr::ByteSet(_) => ExprFlags::POSITIVE,
            Expr::Lookahead(f, _, _)
            | Expr::Not(f, _)
            | Expr::Repeat(f, _, _, _)
            | Expr::Concat(f, _)
            | Expr::Or(f, _)
            | Expr::And(f, _)
            | Expr::ByteConcat(f, _, _) => *f,
        }
    }

    pub fn nullable(&self) -> bool {
        self.get_flags().is_nullable()
    }

    #[inline(always)]
    fn from_slice(s: &'a [u32]) -> Expr<'a> {
        let flags = ExprFlags(s[0] & !0xff);
        let tag = ExprTag::from_u8((s[0] & 0xff) as u8);
        match tag {
            ExprTag::EmptyString => Expr::EmptyString,
            ExprTag::NoMatch => Expr::NoMatch,
            ExprTag::Byte => Expr::Byte(s[1] as u8),
            ExprTag::ByteSet => Expr::ByteSet(&s[1..]),
            ExprTag::Lookahead => Expr::Lookahead(flags, ExprRef::new(s[1]), s[2]),
            ExprTag::Not => Expr::Not(flags, ExprRef::new(s[1])),
            ExprTag::RemainderIs => Expr::RemainderIs {
                divisor: s[1],
                remainder: s[2],
                scale: s[3],
                fractional_part: s[4] != 0,
            },
            ExprTag::Repeat => Expr::Repeat(flags, ExprRef::new(s[1]), s[2], s[3]),
            ExprTag::Concat => Expr::Concat(flags, [ExprRef::new(s[1]), ExprRef::new(s[2])]),
            ExprTag::Or => Expr::Or(flags, bytemuck::cast_slice(&s[1..])),
            ExprTag::And => Expr::And(flags, bytemuck::cast_slice(&s[1..])),
            ExprTag::ByteConcat => {
                let bytes0: &[u8] = bytemuck::cast_slice(&s[2..]);
                let bytes = &bytes0[1..(bytes0[0] + 1) as usize];
                Expr::ByteConcat(flags, bytes, ExprRef::new(s[1]))
            }
        }
    }

    fn serialize(&self, trg: &mut VecHashCons) {
        #[inline(always)]
        fn nary_serialize(trg: &mut VecHashCons, tag: u32, es: &[ExprRef]) {
            trg.push_u32(tag);
            trg.push_slice(bytemuck::cast_slice(es));
        }
        let flags = self.get_flags();
        match self {
            Expr::EmptyString => trg.push_u32(flags.encode(ExprTag::EmptyString)),
            Expr::NoMatch => trg.push_u32(flags.encode(ExprTag::NoMatch)),
            Expr::RemainderIs {
                divisor,
                remainder,
                scale,
                fractional_part,
            } => {
                trg.push_slice(&[
                    flags.encode(ExprTag::RemainderIs),
                    *divisor,
                    *remainder,
                    *scale,
                    *fractional_part as u32,
                ]);
            }
            Expr::Byte(b) => {
                trg.push_slice(&[flags.encode(ExprTag::Byte), *b as u32]);
            }
            Expr::ByteSet(s) => {
                trg.push_u32(flags.encode(ExprTag::ByteSet));
                trg.push_slice(s);
            }
            Expr::Lookahead(flags, e, n) => {
                trg.push_slice(&[flags.encode(ExprTag::Lookahead), e.0, *n]);
            }
            Expr::Not(flags, e) => trg.push_slice(&[flags.encode(ExprTag::Not), e.0]),
            Expr::Repeat(flags, e, a, b) => {
                trg.push_slice(&[flags.encode(ExprTag::Repeat), e.0, *a, *b])
            }
            Expr::Concat(flags, [a, b]) => {
                trg.push_slice(&[flags.encode(ExprTag::Concat), a.0, b.0])
            }
            Expr::Or(flags, es) => nary_serialize(trg, flags.encode(ExprTag::Or), es),
            Expr::And(flags, es) => nary_serialize(trg, flags.encode(ExprTag::And), es),
            Expr::ByteConcat(flags, bytes, tail) => {
                assert!(bytes.len() <= ExprRef::MAX_BYTE_CONCAT);
                let mut buf32 = [0u32; 2 + (ExprRef::MAX_BYTE_CONCAT + 1).div_ceil(4)];
                buf32[0] = flags.encode(ExprTag::ByteConcat);
                buf32[1] = tail.0;
                let buf = bytemuck::cast_slice_mut(&mut buf32[2..]);
                buf[0] = bytes.len() as u8;
                buf[1..(1 + bytes.len())].copy_from_slice(bytes);
                let final_len = 3 + bytes.len() / 4;
                trg.push_slice(&buf32[..final_len]);
            }
        }
    }
}

#[derive(Clone)]
pub struct ExprSet {
    exprs: VecHashCons,
    expr_weight: Vec<(u32, u32)>,
    pub(crate) alphabet_size: usize,
    pub(crate) alphabet_words: usize,
    pub(crate) digits: [u8; 10],
    pub(crate) digit_dot: u8,
    pub(crate) cost: u64,
    pp: PrettyPrinter,
    pub(crate) optimize: bool,
    pub(crate) unicode_cache: HashMap<Vec<(char, char)>, ExprRef>,
    pub(crate) any_unicode_star: ExprRef,
    pub(crate) any_unicode: ExprRef,
    pub(crate) any_unicode_non_nl: ExprRef,
}

const ATTR_HAS_REPEAT: u32 = 1;

impl ExprSet {
    pub fn new(alphabet_size: usize) -> Self {
        let exprs = VecHashCons::new();
        let alphabet_words = alphabet_size.div_ceil(32);
        let mut r = ExprSet {
            exprs,
            expr_weight: vec![],
            alphabet_size,
            alphabet_words,
            digits: [b'0', b'1', b'2', b'3', b'4', b'5', b'6', b'7', b'8', b'9'],
            digit_dot: b'.',
            cost: 0,
            pp: PrettyPrinter::new_simple(alphabet_size),
            optimize: true,
            unicode_cache: HashMap::default(),
            any_unicode: ExprRef::INVALID,
            any_unicode_non_nl: ExprRef::INVALID,
            any_unicode_star: ExprRef::INVALID,
        };

        let id = r.exprs.insert(&[]);
        assert!(id == 0);
        let inserts = vec![
            (r.mk(Expr::EmptyString), ExprRef::EMPTY_STRING),
            (r.mk(Expr::NoMatch), ExprRef::NO_MATCH),
            (
                r.mk(Expr::ByteSet(&vec![0xffffffff; alphabet_words])),
                ExprRef::ANY_BYTE,
            ),
            (
                r.mk_repeat(ExprRef::ANY_BYTE, 0, u32::MAX),
                ExprRef::ANY_BYTE_STRING,
            ),
            (
                r.mk_repeat(ExprRef::ANY_BYTE, 1, u32::MAX),
                ExprRef::NON_EMPTY_BYTE_STRING,
            ),
        ];

        for (x, y) in inserts {
            assert!(x == y, "id: {x:?}, expected: {y:?}");
        }

        r
    }

    /// If this returns true, then the regex will match only strings
    /// starting with the given prefix.
    /// If this returns false, then it's possible (but not sure) it will match something else.
    pub fn has_simply_forced_bytes(&self, e: ExprRef, bytes: &[u8]) -> bool {
        if bytes.is_empty() {
            return true;
        }
        let mut tmp = vec![];
        for a in self.iter_concat(e) {
            a.push_owned_to(&mut tmp);
            if tmp.len() > 1 {
                break;
            }
        }
        match tmp.first() {
            Some(OwnedConcatElement::Bytes(b)) => b.starts_with(bytes),
            _ => false,
        }
    }

    pub fn set_pp(&mut self, pp: PrettyPrinter) {
        self.pp = pp;
    }

    pub fn pp(&self) -> &PrettyPrinter {
        &self.pp
    }

    pub fn cost(&self) -> u64 {
        self.cost
    }

    pub(crate) fn disable_optimizations(&mut self) {
        self.optimize = false;
    }

    pub fn expr_to_string_max_len(&self, id: ExprRef, max_len: usize) -> String {
        self.pp.expr_to_string(self, id, max_len)
    }

    pub fn expr_to_string(&self, id: ExprRef) -> String {
        self.expr_to_string_max_len(id, 1024)
    }

    pub fn expr_to_string_with_info(&self, id: ExprRef) -> String {
        let mut r = self.expr_to_string(id);
        r.push_str(&self.pp.alphabet_info());
        r
    }

    pub fn len(&self) -> usize {
        self.exprs.len()
    }

    pub fn is_empty(&self) -> bool {
        self.exprs.is_empty()
    }

    pub fn num_bytes(&self) -> usize {
        self.exprs.num_bytes()
    }

    fn compute_weight(&mut self, e: ExprRef) -> u32 {
        let mut todo = vec![e];
        let mut mapped = Vec::with_capacity(32);
        while let Some(e) = todo.pop() {
            if self.get_cached_attrs(e).0 != 0 {
                continue;
            }
            mapped.clear();
            let mut needs_more_work = false;
            let mut flags = 0;
            for &c in self.get_args(e) {
                let w = self.get_cached_attrs(c);
                flags |= w.1;
                if w.0 == 0 {
                    if !needs_more_work {
                        todo.push(e);
                        needs_more_work = true;
                    }
                    todo.push(c);
                } else {
                    mapped.push(w);
                }
            }
            if needs_more_work {
                continue;
            }

            let w = match self.get(e) {
                Expr::EmptyString => 1,
                Expr::NoMatch => 1,
                Expr::Byte(_) => 1,
                Expr::ByteSet(_) => 2,
                Expr::RemainderIs { .. } => 100,
                Expr::Lookahead(_, _, _) => mapped[0].0 + 1,
                Expr::Not(_, _) => mapped[0].0 + 50,
                Expr::Repeat(_, _, min, max) => {
                    if max >= 2 {
                        flags |= ATTR_HAS_REPEAT;
                    }
                    mapped[0].0 + std::cmp::min(min, 10) + std::cmp::min(max, 10)
                }
                Expr::Concat(_, _) => mapped[0].0 + mapped[1].0 + 1,
                Expr::Or(_, _) => mapped.iter().map(|e| e.0).sum(),
                Expr::And(_, _) => mapped.iter().map(|e| e.0).sum::<u32>() + 20,
                Expr::ByteConcat(_, items, _) => items.len() as u32 + mapped[0].0,
            };

            // if we hit a very large size (likely to due to DAG-like structure of the regex),
            // switch to a mode where we compute depth of the regex tree, not size
            let off = 1_000_000;
            let w = if w < off {
                w
            } else {
                std::cmp::max(
                    off,
                    match self.get(e) {
                        Expr::Concat(_, _) => std::cmp::max(mapped[0].0, mapped[1].0) + 1,
                        Expr::Or(_, _) => mapped.iter().map(|e| e.0).max().unwrap() + 5,
                        Expr::And(_, _) => mapped.iter().map(|e| e.0).max().unwrap() + 20,
                        _ => w,
                    },
                )
            };

            let idx = e.0 as usize;
            if idx >= self.expr_weight.len() {
                self.expr_weight.resize(idx + 100, (0, 0));
            }
            self.expr_weight[idx] = (w, flags);
        }
        0
    }

    // When called outside ctor, one should also call self.pay()
    pub(crate) fn mk(&mut self, e: Expr) -> ExprRef {
        self.exprs.start_insert();
        e.serialize(&mut self.exprs);
        ExprRef(self.exprs.finish_insert())
    }

    fn get_cached_attrs(&self, id: ExprRef) -> (u32, u32) {
        self.expr_weight
            .get(id.0 as usize)
            .copied()
            .unwrap_or((0, 0))
    }

    fn get_attrs(&mut self, id: ExprRef) -> (u32, u32) {
        let mut r = self.get_cached_attrs(id);
        if r.0 == 0 {
            self.compute_weight(id);
            r = self.get_cached_attrs(id);
        }
        r
    }

    pub fn get_weight(&mut self, id: ExprRef) -> u32 {
        self.get_attrs(id).0
    }

    fn get_attr_flags(&mut self, id: ExprRef) -> u32 {
        self.get_attrs(id).1
    }

    pub fn attr_has_repeat(&mut self, id: ExprRef) -> bool {
        let w = self.get_attr_flags(id);
        (w & ATTR_HAS_REPEAT) != 0
    }

    pub fn get(&self, id: ExprRef) -> Expr {
        Expr::from_slice(self.exprs.get(id.0))
    }

    pub fn reserve(&mut self, size: usize) {
        self.exprs.reserve(size);
    }

    pub(crate) fn get_bytes(&self, id: ExprRef) -> Option<&[u8]> {
        let slice = self.exprs.get(id.0);
        match Expr::from_slice(slice) {
            Expr::Byte(_) => {
                let bslice: &[u8] = bytemuck::cast_slice(&slice[1..2]);
                Some(&bslice[0..1])
            }
            Expr::ByteConcat(_, bytes, _) => Some(bytes),
            _ => None,
        }
    }

    pub fn is_valid(&self, id: ExprRef) -> bool {
        id.is_valid() && self.exprs.is_valid(id.0)
    }

    fn lookahead_len_inner(&self, e: ExprRef) -> Option<usize> {
        match self.get(e) {
            Expr::Lookahead(_, ExprRef::EMPTY_STRING, n) => Some(n as usize),
            _ => None,
        }
    }

    pub fn lookahead_len(&self, e: ExprRef) -> Option<usize> {
        match self.get(e) {
            Expr::Or(_, args) => args
                .iter()
                .filter_map(|&arg| self.lookahead_len_inner(arg))
                .min(),
            _ => self.lookahead_len_inner(e),
        }
    }

    fn possible_lookahead_len_inner(&self, e: ExprRef) -> usize {
        match self.get(e) {
            Expr::Lookahead(_, _, n) => n as usize,
            _ => 0,
        }
    }

    pub fn possible_lookahead_len(&self, e: ExprRef) -> usize {
        match self.get(e) {
            Expr::Or(_, args) => args
                .iter()
                .map(|&arg| self.possible_lookahead_len_inner(arg))
                .max()
                .unwrap_or(0),
            _ => self.possible_lookahead_len_inner(e),
        }
    }

    pub fn get_flags(&self, id: ExprRef) -> ExprFlags {
        assert!(id.is_valid());
        ExprFlags(self.exprs.get(id.0)[0] & !0xff)
    }

    pub fn get_tag(&self, id: ExprRef) -> ExprTag {
        assert!(id.is_valid());
        let tag = self.exprs.get(id.0)[0] & 0xff;
        ExprTag::from_u8(tag as u8)
    }

    pub fn get_args(&self, id: ExprRef) -> &[ExprRef] {
        let s = self.exprs.get(id.0);
        let tag = ExprTag::from_u8((s[0] & 0xff) as u8);
        match tag {
            ExprTag::Concat | ExprTag::Or | ExprTag::And => bytemuck::cast_slice(&s[1..]),
            ExprTag::Not | ExprTag::Repeat | ExprTag::Lookahead | ExprTag::ByteConcat => {
                bytemuck::cast_slice(&s[1..2])
            }
            ExprTag::RemainderIs
            | ExprTag::EmptyString
            | ExprTag::NoMatch
            | ExprTag::Byte
            | ExprTag::ByteSet => &[],
        }
    }

    pub fn is_nullable(&self, id: ExprRef) -> bool {
        self.get_flags(id).is_nullable()
    }

    pub fn is_positive(&self, id: ExprRef) -> bool {
        self.get_flags(id).is_positive()
    }

    #[inline(always)]
    pub fn simple_map<V: Clone>(
        &mut self,
        r: ExprRef,
        process: impl FnMut(&mut ExprSet, &mut Vec<V>, ExprRef) -> V,
    ) -> V {
        let mut cache = HashMap::default();
        let concat_nullable_check = false;
        self.map(r, &mut cache, concat_nullable_check, |e| e, process)
    }

    #[inline(always)]
    pub fn map<K: Eq + PartialEq + Hash, V: Clone>(
        &mut self,
        r: ExprRef,
        cache: &mut HashMap<K, V>,
        concat_nullable_check: bool,
        mk_key: impl Fn(ExprRef) -> K,
        mut process: impl FnMut(&mut ExprSet, &mut Vec<V>, ExprRef) -> V,
    ) -> V {
        if let Some(d) = cache.get(&mk_key(r)) {
            return d.clone();
        }

        let mut todo = vec![r];
        let mut mapped = Vec::with_capacity(128);

        while let Some(r) = todo.last() {
            let r = *r;
            let idx = mk_key(r);
            if cache.contains_key(&idx) {
                todo.pop();
                continue;
            }
            let e = self.get(r);
            let is_concat = concat_nullable_check && matches!(e, Expr::Concat(_, _));
            let is_byte_concat = concat_nullable_check && matches!(e, Expr::ByteConcat(_, _, _));
            let todo_len = todo.len();
            let eargs = e.args();
            mapped.clear();
            if !is_byte_concat {
                for a in eargs {
                    let a = *a;
                    let brk = is_concat && !self.is_nullable(a);
                    if let Some(v) = cache.get(&mk_key(a)) {
                        mapped.push(v.clone());
                    } else {
                        todo.push(a);
                    }
                    if brk {
                        break;
                    }
                }
            }

            if todo.len() != todo_len {
                continue; // retry children first
            }

            todo.pop(); // pop r

            let v = process(self, &mut mapped, r);
            cache.insert(idx, v);
        }
        cache[&mk_key(r)].clone()
    }
}

#[derive(Clone, Copy, PartialEq, Eq)]
pub enum NextByte {
    /// Transition via any other byte, or EOI leads to a dead state.
    ForcedByte(u8),
    /// Transition via any byte leads to a dead state but EOI is possible.
    ForcedEOI,
    /// Transition via some bytes *may be* possible.
    /// The bytes are possible examples.
    SomeBytes0,
    SomeBytes1(u8),
    SomeBytes2([u8; 2]),
    /// The current state is dead.
    /// Should be only true for NO_MATCH.
    Dead,
}

impl Debug for NextByte {
    fn fmt(&self, f: &mut std::fmt::Formatter<'_>) -> std::fmt::Result {
        match self {
            NextByte::ForcedByte(b) => write!(f, "ForcedByte({:?})", *b as char),
            NextByte::ForcedEOI => write!(f, "ForcedEOI"),
            NextByte::SomeBytes0 => write!(f, "SomeBytes0"),
            NextByte::SomeBytes1(b) => write!(f, "SomeBytes1({:?})", *b as char),
            NextByte::SomeBytes2([a, b]) => {
                write!(f, "SomeBytes2({:?}, {:?})", *a as char, *b as char)
            }
            NextByte::Dead => write!(f, "Dead"),
        }
    }
}

impl NextByte {
    pub fn some_bytes(&self) -> &[u8] {
        match self {
            NextByte::ForcedByte(b) => std::slice::from_ref(b),
            NextByte::SomeBytes1(b) => std::slice::from_ref(b),
            NextByte::SomeBytes2(b) => b,
            _ => &[],
        }
    }

    pub fn is_some_bytes(&self) -> bool {
        matches!(
            self,
            NextByte::SomeBytes0 | NextByte::SomeBytes1(_) | NextByte::SomeBytes2(_)
        )
    }

    pub fn some_bytes_from_slice(s: &[u8]) -> Self {
        match s.len() {
            0 => NextByte::SomeBytes0,
            1 => NextByte::SomeBytes1(s[0]),
            _ => NextByte::SomeBytes2([s[0], s[1]]),
        }
    }

    pub fn make_fuzzy(&self) -> Self {
        match self {
            NextByte::ForcedByte(a) => NextByte::SomeBytes1(*a),
            NextByte::ForcedEOI => NextByte::SomeBytes0,
            _ => *self,
        }
    }
}

impl BitOr for NextByte {
    type Output = Self;
    fn bitor(self, other: Self) -> Self {
        match (self, other) {
            (NextByte::Dead, _) => other,
            (_, NextByte::Dead) => self,
            (NextByte::ForcedByte(a), NextByte::ForcedByte(b)) => {
                if a == b {
                    self
                } else {
                    NextByte::SomeBytes2([a, b])
                }
            }
            (NextByte::ForcedEOI, NextByte::ForcedEOI) => self,
            _ => {
                let a = self.some_bytes();
                let b = other.some_bytes();
                if a.is_empty() || b.len() > 1 {
                    NextByte::some_bytes_from_slice(b)
                } else if b.is_empty() || a.len() > 1 {
                    NextByte::some_bytes_from_slice(a)
                } else {
                    let a = a[0];
                    let b = b[0];
                    if a == b {
                        NextByte::SomeBytes1(a)
                    } else {
                        NextByte::SomeBytes2([a, b])
                    }
                }
            }
        }
    }
}
