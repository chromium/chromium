use crate::{HashMap, HashSet};
use anyhow::Result;
use std::hash::Hash;

use crate::{
    ast::{Expr, ExprRef, ExprSet},
    nextbyte::next_byte_simple,
    raw::DerivCache,
    simplify::{ConcatElement, OwnedConcatElement},
    NextByte,
};

// This is map (ByteSet => RegExp); ByteSet is Expr::Byte or Expr::ByteSet,
// and expresses the condition; RegExp is the derivative under that condition.
// Conditions can be overlapping, but should not repeat.
//
// deriv(R) = [(S0,C0),...,(Sn,Cn)]
//   means that
// deriv(byte, R) = Ca | Cb ... when byte ∈ Sa and byte ∈ Sb[byte]
// There is an implicit Sn+1 = Σ \ ⋃Si with Cn+1 = NO_MATCH
type SymRes = Vec<(ExprRef, ExprRef)>;

const DEBUG: bool = false;
macro_rules! debug {
    ($($arg:tt)*) => {
        if DEBUG {
            eprint!("  ");
            eprintln!($($arg)*);
        }
    };
}

#[derive(Clone)]
pub struct RelevanceCache {
    relevance_cache: HashMap<ExprRef, bool>,
    containment_cache: HashMap<(ExprRef, ExprRef), bool>,
    sym_deriv: HashMap<ExprRef, SymRes>,
    max_fuel: u64,
    cost_limit: u64,
}

fn swap_each<A: Copy>(v: &mut [(A, A)]) {
    for elem in v.iter_mut() {
        *elem = (elem.1, elem.0);
    }
}

fn group_by_first<A: PartialEq + Ord + Copy + Hash, B: Ord + Copy>(
    s: Vec<(A, B)>,
    mut f: impl FnMut(&mut Vec<B>) -> B,
) -> Vec<(A, B)> {
    let mut first_idx: HashMap<A, usize> = HashMap::default();
    let mut by_set: Vec<(A, Vec<B>)> = vec![];
    let mut dupl = false;

    for (a, _) in &s {
        if first_idx.contains_key(a) {
            dupl = true;
        } else {
            first_idx.insert(*a, by_set.len());
        }
    }

    if !dupl {
        return s;
    }

    first_idx.clear();
    for (a, b) in &s {
        if let Some(idx) = first_idx.get(a) {
            by_set[*idx].1.push(*b);
        } else {
            first_idx.insert(*a, by_set.len());
            by_set.push((*a, vec![*b]));
        }
    }

    by_set
        .into_iter()
        .map(|(a, mut bs)| {
            if bs.len() == 1 {
                (a, bs[0])
            } else {
                (a, f(&mut bs))
            }
        })
        .collect()
}

fn simplify(exprs: &mut ExprSet, s: SymRes) -> SymRes {
    if s.len() <= 1 {
        return s;
    }

    if s.len() == 2 && s[0].0 != s[1].0 {
        return s;
    }

    exprs.pay(s.len());
    let mut s = group_by_first(s, |args| exprs.mk_or(args));
    swap_each(&mut s);
    let mut s = group_by_first(s, |args| exprs.mk_byte_set_or(args));
    swap_each(&mut s);
    s
}

fn make_disjoint(exprs: &mut ExprSet, inp: &SymRes) -> SymRes {
    // invariant: bytesets in res are all disjoint
    let mut res: SymRes = vec![];
    'input: for (mut a, av) in inp {
        let av = *av;
        // only iterate over items from previous step
        // for each previous item, intersect it with the current item
        // and store the results if any
        let len0 = res.len();
        for idx in 0..len0 {
            let (b, bv) = res[idx];

            // we've got lucky - direct match
            if a == b {
                res[idx] = (a, exprs.mk_or(&mut vec![av, bv]));
                continue 'input;
            }

            let a_and_b = exprs.mk_byte_set_and(a, b);
            if a_and_b == ExprRef::NO_MATCH {
                // this item doesn't intersect with the current one
                continue;
            }

            // replace previous b with a&b -> av|ab
            res[idx] = (a_and_b, exprs.mk_or(&mut vec![av, bv]));
            let b_sub_a = exprs.mk_byte_set_sub(b, a);
            if b_sub_a != ExprRef::NO_MATCH {
                // also add b-a -> bv (if non-empty)
                res.push((b_sub_a, bv));
            }

            // a&b was already handled - remove from a
            a = exprs.mk_byte_set_sub(a, a_and_b);
            if a == ExprRef::NO_MATCH {
                continue 'input;
            }
        }

        assert!(a != ExprRef::NO_MATCH);
        // store the left-overs
        res.push((a, av));
    }

    res
}

impl Default for RelevanceCache {
    fn default() -> Self {
        Self::new()
    }
}

impl RelevanceCache {
    pub fn new() -> Self {
        RelevanceCache {
            relevance_cache: HashMap::default(),
            sym_deriv: HashMap::default(),
            containment_cache: HashMap::default(),
            cost_limit: u64::MAX,
            max_fuel: u64::MAX,
        }
    }

    pub fn num_bytes(&self) -> usize {
        self.relevance_cache.len() * 3 * std::mem::size_of::<isize>()
    }

    fn deriv(&mut self, exprs: &mut ExprSet, e: ExprRef) -> SymRes {
        exprs.map(
            e,
            &mut self.sym_deriv,
            true,
            |e| e,
            |exprs, deriv, e| {
                let r = match exprs.get(e) {
                    Expr::EmptyString => vec![],
                    Expr::NoMatch => vec![],
                    Expr::Byte(_) => vec![(e, ExprRef::EMPTY_STRING)],
                    Expr::ByteSet(_) => vec![(e, ExprRef::EMPTY_STRING)],
                    Expr::ByteConcat(_, bytes, tail) => {
                        let bb = bytes[1..].to_vec();
                        let sel = exprs.mk_byte(bytes[0]);
                        let d = exprs.mk_byte_concat(&bb, tail);
                        vec![(sel, d)]
                    }

                    // just unwrap lookaheads
                    Expr::Lookahead(_, _, _) => deriv.pop().unwrap(),

                    Expr::RemainderIs {
                        divisor,
                        remainder,
                        scale,
                        fractional_part,
                    } => {
                        let mut result = vec![];
                        for i in 0..10 {
                            let b = exprs.mk_byte(exprs.digits[i]);
                            let (remainder, scale) = if !fractional_part {
                                (remainder * 10, scale)
                            } else {
                                if scale == 0 {
                                    // Dead code?
                                    result.push((b, ExprRef::NO_MATCH));
                                    continue;
                                }
                                (remainder, scale - 1)
                            };
                            let r = exprs.mk_remainder_is(
                                divisor,
                                (remainder + (i as u32) * 10_u32.pow(scale)) % divisor,
                                scale,
                                fractional_part,
                            );
                            result.push((b, r));
                        }
                        if !fractional_part && scale > 0 {
                            let b = exprs.mk_byte(exprs.digit_dot);
                            let r = exprs.mk_remainder_is(divisor, remainder, scale, true);
                            result.push((b, r));
                        }
                        result
                    }
                    Expr::And(_, _) => {
                        let mut acc = deriv.pop().unwrap();
                        while let Some(other) = deriv.pop() {
                            let mut new_acc = vec![];
                            for (b0, r0) in &acc {
                                for (b1, r1) in &other {
                                    let b = exprs.mk_byte_set_and(*b0, *b1);
                                    if b != ExprRef::NO_MATCH {
                                        debug!(
                                            "  and: {} & {}",
                                            exprs.expr_to_string(*r0),
                                            exprs.expr_to_string(*r1)
                                        );
                                        let r = exprs.mk_and(&mut vec![*r0, *r1]);
                                        if r != ExprRef::NO_MATCH {
                                            new_acc.push((b, r));
                                        }
                                    }
                                }
                            }
                            acc = new_acc;
                        }
                        simplify(exprs, acc)
                    }

                    Expr::Or(_, _) => simplify(exprs, deriv.drain(0..).flatten().collect()),

                    Expr::Not(_, _) => {
                        let inner_deriv = make_disjoint(exprs, &deriv[0]);
                        debug!(
                            "  disjoint: ==> {:?}",
                            inner_deriv
                                .iter()
                                .map(|(b, r)| (exprs.expr_to_string(*b), exprs.expr_to_string(*r)))
                                .collect::<Vec<_>>()
                        );
                        let mut negated_deriv = inner_deriv
                            .iter()
                            .map(|(b, r)| (*b, exprs.mk_not(*r)))
                            .collect::<Vec<_>>();
                        let left_over = exprs.mk_byte_set_neg_or(
                            &deriv[0].iter().map(|(b, _)| *b).collect::<Vec<_>>(),
                        );
                        if left_over != ExprRef::NO_MATCH {
                            negated_deriv.push((left_over, ExprRef::ANY_BYTE_STRING));
                        }
                        simplify(exprs, negated_deriv)
                    }

                    Expr::Repeat(_, e, min, max) => {
                        let max = if max == u32::MAX {
                            u32::MAX
                        } else {
                            max.saturating_sub(1)
                        };
                        let tail = exprs.mk_repeat(e, min.saturating_sub(1), max);
                        let tmp = deriv[0]
                            .iter()
                            .map(|(b, r)| (*b, exprs.mk_concat(*r, tail)))
                            .collect();
                        simplify(exprs, tmp)
                    }
                    Expr::Concat(_, [aa, bb]) => {
                        let mut or_branches: Vec<_> = deriv[0]
                            .iter()
                            .map(|(b, r)| (*b, exprs.mk_concat(*r, bb)))
                            .collect();
                        if exprs.is_nullable(aa) {
                            or_branches.extend_from_slice(&deriv[1]);
                        }
                        simplify(exprs, or_branches)
                    }
                };
                exprs.pay(r.len());
                debug!(
                    "deriv: {:?} -> {:?}",
                    exprs.expr_to_string(e),
                    r.iter()
                        .map(|(b, r)| (exprs.expr_to_string(*b), exprs.expr_to_string(*r)))
                        .collect::<Vec<_>>()
                );
                r
            },
        )
    }

    pub fn is_non_empty(&mut self, exprs: &mut ExprSet, top_expr: ExprRef) -> bool {
        self.is_non_empty_limited(exprs, top_expr, u64::MAX)
            .unwrap()
    }

    fn check_contains(
        &mut self,
        exprs: &mut ExprSet,
        small: ExprRef,
        big: ExprRef,
    ) -> Result<bool> {
        if small == big {
            return Ok(true);
        }
        let not_big = exprs.mk_not(big);
        let cont = exprs.mk_and2(small, not_big);
        Ok(!(self.is_non_empty_inner(exprs, cont)?))
    }

    fn is_contained_in_prefixes_repeats(
        &mut self,
        exprs: &mut ExprSet,
        small: ExprRef,
        main: ExprRef,
        except: ExprRef,
    ) -> Result<bool> {
        match (exprs.get(small), exprs.get(main)) {
            (Expr::Repeat(_, small_ch, _, small_high), Expr::Repeat(_, main_ch, _, main_high))
                if small_high <= main_high && 2 <= main_high =>
            {
                if !or_branches(exprs, &[except]).iter().all(|c| {
                    simple_max_length(exprs, *c).unwrap_or(usize::MAX) < main_high as usize
                }) {
                    debug!(" -> len, nonempty");
                    return Ok(false);
                }

                debug!(
                    "check {} in {}",
                    exprs.expr_to_string(small_ch),
                    exprs.expr_to_string(main_ch)
                );

                if self.check_contains(exprs, small_ch, main_ch)? {
                    debug!(" -> rec, empty");
                } else {
                    debug!(" -> rec, nonempty");
                    return Ok(false);
                }

                debug!(" -> empty");
                Ok(true)
            }
            _ => {
                debug!(" -> no repeat");
                Ok(false)
            }
        }
    }

    fn is_contained_in_prefixes_inner(
        &mut self,
        exprs: &mut ExprSet,
        deriv: &mut DerivCache,
        mut small: ExprRef,
        mut big: ExprRef,
    ) -> Result<bool> {
        debug!(
            "check contained {} in {}",
            exprs.expr_to_string(small),
            exprs.expr_to_string(big)
        );

        // rewind through any initial forced bytes
        for _ in 0..10 {
            match next_byte_simple(exprs, small) {
                NextByte::ForcedByte(b) => {
                    small = deriv.derivative(exprs, small, b);
                    big = deriv.derivative(exprs, big, b);
                }
                _ => break,
            }
        }

        // split big into main & ~except
        let (main, except) = a_and_not_b(exprs, big).unwrap_or((big, ExprRef::NO_MATCH));

        // if main is of the form X? Y transform it to Y
        let main = match exprs.get(main) {
            Expr::Concat(_, [a, b]) => match exprs.get(a) {
                Expr::Repeat(_, _, 0, 1) => b,
                _ => main,
            },
            _ => main,
        };

        match exprs.get(small) {
            // small = [1-9][0-9]*
            Expr::Concat(_, [hd, small2])
                if matches!(exprs.get(small2), Expr::Repeat(_, _, _, _)) =>
            {
                let mut left_overs = vec![];
                let mut stack = vec![main];

                while let Some(main) = stack.pop() {
                    for &main_b in or_branches(exprs, &[main]) {
                        debug!(
                            "  -> check branch {} in {}",
                            exprs.expr_to_string(hd),
                            exprs.expr_to_string(main_b)
                        );
                        match exprs.get(main_b) {
                            // if we're in [0-9]* phase of number regex
                            Expr::Repeat(_, main_ch, _, max_rep) if max_rep > 1 => {
                                // process is later
                                left_overs.push((main_ch, max_rep))
                            }
                            Expr::Concat(_, [hd2, main2]) => {
                                if hd == hd2 {
                                    // if we're in [1-9][0-9]* phase, skip the [1-9]
                                    // and do check on the rest
                                    // note that 'except' matching is only done length-wise
                                    // so this is conservative
                                    return self.is_contained_in_prefixes_repeats(
                                        exprs, small2, main2, except,
                                    );
                                } else {
                                    debug!("  -> in concat {}", exprs.expr_to_string(hd2));
                                    stack.push(hd2);
                                }
                            }
                            _ => {}
                        }
                    }
                }

                for (main_ch, max_rep) in left_overs {
                    if self.check_contains(exprs, hd, main_ch)? {
                        let new_rep = if max_rep == u32::MAX {
                            max_rep
                        } else {
                            max_rep - 1
                        };
                        let main2 = exprs.mk_repeat(main_ch, 0, new_rep);
                        if self.is_contained_in_prefixes_repeats(exprs, small2, main2, except)? {
                            return Ok(true);
                        }
                    }
                }
                return Ok(false);
            }
            _ => {}
        }

        // if main is an alternative, pick the one which is repeat
        let main = match exprs.get(main) {
            Expr::Or(_, ch) => ch
                .iter()
                .find(|&a| matches!(exprs.get(*a), Expr::Repeat(_, _, _, _)))
                .copied()
                .unwrap_or(main),
            _ => main,
        };

        // if (main, except) are of the form (main suffix, except suffix), then
        // strip the suffix
        let (main, except) = strip_common_suffix(exprs, main, except);

        self.is_contained_in_prefixes_repeats(exprs, small, main, except)
    }

    /// Check if `small` is contained in `big` with a limit on the number of steps.
    /// If `cache_failures` is true, then the result of the check is cached,
    /// as `false` in case of error (so future checks will return false not error,
    /// even if max_fuel is increased).
    pub fn is_contained_in_prefixes(
        &mut self,
        exprs: &mut ExprSet,
        deriv: &mut DerivCache,
        small: ExprRef,
        big: ExprRef,
        max_fuel: u64,
        cache_failures: bool,
    ) -> Result<bool> {
        if let Some(r) = self.containment_cache.get(&(small, big)) {
            return Ok(*r);
        }

        self.max_fuel = max_fuel;
        self.cost_limit = exprs.cost().saturating_add(max_fuel);

        match self.is_contained_in_prefixes_inner(exprs, deriv, small, big) {
            Ok(r) => {
                self.containment_cache.insert((small, big), r);
                Ok(r)
            }
            Err(e) => {
                if cache_failures {
                    self.containment_cache.insert((small, big), false);
                }
                Err(e)
            }
        }
    }

    pub fn is_non_empty_limited(
        &mut self,
        exprs: &mut ExprSet,
        top_expr: ExprRef,
        max_fuel: u64,
    ) -> Result<bool> {
        self.max_fuel = max_fuel;
        let cost_0 = exprs.cost();
        self.cost_limit = cost_0.saturating_add(max_fuel);
        let r = self.is_non_empty_inner(exprs, top_expr);
        if false {
            println!("cost: {}", exprs.cost() - cost_0);
        }
        r
    }

    fn is_non_empty_inner(&mut self, exprs: &mut ExprSet, top_expr: ExprRef) -> Result<bool> {
        if exprs.is_positive(top_expr) {
            return Ok(true);
        }
        if let Some(r) = self.relevance_cache.get(&top_expr) {
            return Ok(*r);
        }

        // println!("relevance: {}", exprs.expr_to_string(top_expr));

        match exprs.get(top_expr) {
            Expr::Concat(_, _) | Expr::ByteConcat(_, _, _) => {
                let mut tmp = vec![];
                let mut num_filter = 0;
                for a in exprs.iter_concat(top_expr) {
                    match a {
                        ConcatElement::Expr(e) => {
                            if exprs.is_positive(e) {
                                num_filter += 1;
                            } else {
                                tmp.push(OwnedConcatElement::Expr(e));
                            }
                        }
                        ConcatElement::Bytes(_) => {
                            num_filter += 1;
                        }
                    }
                }

                if num_filter > 0 {
                    let inner = exprs._mk_concat_vec(tmp);
                    return self.is_non_empty_inner(exprs, inner);
                }
            }
            Expr::And(_, &[e0, e1]) => {
                if let (Expr::Repeat(_, e0, min0, max0), Expr::Repeat(_, e1, min1, max1)) =
                    (exprs.get(e0), exprs.get(e1))
                {
                    let min2 = std::cmp::max(min0, min1);
                    let max2 = std::cmp::min(max0, max1);
                    if min2 <= max2 {
                        // ranges intersect
                        let e2 = exprs.mk_and(&mut vec![e0, e1]);
                        if let Ok(true) = self.is_non_empty_inner(exprs, e2) {
                            self.relevance_cache.insert(top_expr, true);
                            return Ok(true);
                        }
                    }
                }
            }
            _ => {}
        }

        /*
        DFS:
            is_relevant(e)
                if positive(e): return true
                if C[e] is set: return C[e]
                if pending[e]: return false

                pending[e] = true
                for (_, c) in sym_deriv(e)
                    if is_relevant(c)
                        C[e] := true
                        return true
                pending[e] = false
                C[e] := false
                return false
        */

        struct StackFrame {
            expr: ExprRef,
            curr_idx: usize,
            children: Vec<ExprRef>,
        }

        let mut visited = HashSet::default();
        let mut stack = vec![StackFrame {
            expr: top_expr, // not true actually, but doesn't matter
            curr_idx: 0,
            children: vec![top_expr],
        }];

        debug!("\nstart relevance: {}", exprs.expr_to_string(top_expr));

        while !stack.is_empty() {
            let s = stack.last_mut().unwrap();
            if s.curr_idx >= s.children.len() {
                stack.pop();
                continue;
            }
            let curr_node = s.children[s.curr_idx];
            s.curr_idx += 1;
            if visited.contains(&curr_node) {
                continue;
            }
            let status = self.relevance_cache.get(&curr_node);
            if status == Some(&false) {
                continue;
            }

            if status == Some(&true) || exprs.is_positive(curr_node) {
                while let Some(e) = stack.pop() {
                    debug!("  mark relevant: {}", exprs.expr_to_string(e.expr));
                    self.relevance_cache.insert(e.expr, true);
                }
                return Ok(true);
            }

            let dr = self.deriv(exprs, curr_node);
            exprs.pay(dr.len());

            visited.insert(curr_node);
            let mut f = StackFrame {
                expr: curr_node,
                curr_idx: 0,
                children: dr
                    .iter()
                    .map(|e| e.1)
                    .filter(|e| !visited.contains(e))
                    .collect(),
            };

            f.children
                .sort_unstable_by_key(|&e| (exprs.get_weight(e), e.0));

            debug!("  push: {}", exprs.expr_to_string(curr_node));
            for &c in &f.children {
                debug!("    child: {}", exprs.expr_to_string(c));
            }

            if !f.children.is_empty() {
                stack.push(f);
            }

            anyhow::ensure!(
                exprs.cost() <= self.cost_limit,
                "maximum relevance check fuel {} exceeded",
                self.max_fuel
            );
        }

        // we didn't find anything relevant, everything visited is not relevant
        for &e in &visited {
            debug!("  mark empty: {}", exprs.expr_to_string(e));
            self.relevance_cache.insert(e, false);
        }

        Ok(false)
    }
}

fn a_and_not_b(exprs: &ExprSet, e: ExprRef) -> Option<(ExprRef, ExprRef)> {
    match exprs.get(e) {
        Expr::And(_, [a1, a2]) => match (exprs.get(*a1), exprs.get(*a2)) {
            (Expr::Not(_, a), _) => Some((*a2, a)),
            (_, Expr::Not(_, a)) => Some((*a1, a)),
            _ => None,
        },
        _ => None,
    }
}

fn or_branches<'a>(exprs: &'a ExprSet, e: &'a [ExprRef]) -> &'a [ExprRef] {
    assert!(e.len() == 1);
    match exprs.get(e[0]) {
        Expr::Or(_, args) => args,
        Expr::NoMatch => &[],
        _ => e,
    }
}

fn simple_max_length(exprs: &ExprSet, e: ExprRef) -> Option<usize> {
    match exprs.get(e) {
        Expr::ByteSet(_) | Expr::Byte(_) => Some(1),
        Expr::EmptyString => Some(0),
        Expr::Or(_, args) => {
            let mut mx = 0;
            for a in args {
                if let Some(l) = simple_max_length(exprs, *a) {
                    mx = mx.max(l);
                } else {
                    return None;
                }
            }
            Some(mx)
        }
        Expr::Concat(_, _) | Expr::ByteConcat(_, _, _) => {
            let mut sum = 0;
            for a in exprs.iter_concat(e) {
                match a {
                    ConcatElement::Bytes(b) => sum += b.len(),
                    ConcatElement::Expr(a) => {
                        if let Some(l) = simple_max_length(exprs, a) {
                            sum += l;
                        } else {
                            return None;
                        }
                    }
                }
            }
            Some(sum)
        }
        _ => None,
    }
}

fn strip_common_suffix(exprs: &ExprSet, a: ExprRef, b: ExprRef) -> (ExprRef, ExprRef) {
    match (exprs.get(a), exprs.get(b)) {
        (Expr::Concat(_, [a0, a1]), Expr::Concat(_, [b0, b1])) if a1 == b1 => (a0, b0),
        (Expr::Concat(_, [a0, _]), _) => (a0, b),
        _ => (a, b),
    }
}
