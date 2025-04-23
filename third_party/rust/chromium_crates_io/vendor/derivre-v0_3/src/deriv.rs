use crate::HashMap;

use crate::ast::{Expr, ExprRef, ExprSet};

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
pub struct DerivCache {
    pub num_deriv: usize,
    state_table: HashMap<(ExprRef, u8), ExprRef>,
}

impl Default for DerivCache {
    fn default() -> Self {
        Self::new()
    }
}

impl DerivCache {
    pub fn new() -> Self {
        DerivCache {
            num_deriv: 0,
            state_table: HashMap::default(),
        }
    }

    pub fn derivative(&mut self, exprs: &mut ExprSet, r: ExprRef, b: u8) -> ExprRef {
        // This kicks in for lexers with lots of keywords, that is regexps that are
        // just concats of single bytes.
        // Most of these do not match, so this provides very significant speedup.
        // TODO add a flag on exprs to see if this even applies?
        match exprs.get(r) {
            Expr::Concat(_, args) => {
                if exprs.get(args[0]).surely_no_match(b) {
                    return ExprRef::NO_MATCH;
                }
            }
            // Expr::Repeat(_, e, _, _) => {
            //     if exprs.get(e).surely_no_match(b) {
            //         return ExprRef::NO_MATCH;
            //     }
            // }
            e => {
                if e.surely_no_match(b) {
                    return ExprRef::NO_MATCH;
                }
            }
        }

        let mut or_branches = vec![];

        // regular path
        exprs.map(
            r,
            &mut self.state_table,
            true,
            |r| (r, b),
            |exprs, deriv, r| {
                let e = exprs.get(r);
                self.num_deriv += 1;
                let d = match e {
                    Expr::EmptyString | Expr::NoMatch | Expr::ByteSet(_) | Expr::Byte(_) => {
                        if e.matches_byte(b) {
                            ExprRef::EMPTY_STRING
                        } else {
                            ExprRef::NO_MATCH
                        }
                    }
                    Expr::ByteConcat(_, bytes, tail) => {
                        if bytes[0] == b {
                            let copy = bytes[1..].to_vec();
                            exprs.mk_byte_concat(&copy, tail)
                        } else {
                            ExprRef::NO_MATCH
                        }
                    }
                    Expr::RemainderIs {
                        divisor,
                        remainder,
                        scale,
                        fractional_part,
                    } => {
                        if let Some(idx) = exprs.digits.iter().position(|&x| x == b) {
                            let (remainder, scale) = if !fractional_part {
                                (remainder * 10, scale)
                            } else {
                                if scale == 0 {
                                    // Dead code?
                                    return ExprRef::NO_MATCH;
                                }
                                (remainder, scale - 1)
                            };
                            exprs.mk_remainder_is(
                                divisor,
                                (remainder + (idx as u32) * 10_u32.pow(scale)) % divisor,
                                scale,
                                fractional_part,
                            )
                        } else if b == exprs.digit_dot && !fractional_part && scale > 0 {
                            exprs.mk_remainder_is(divisor, remainder, scale, true)
                        } else {
                            ExprRef::NO_MATCH
                        }
                    }
                    Expr::And(_, _) => exprs.mk_and(deriv),
                    Expr::Or(_, _) => exprs.mk_or(deriv),
                    Expr::Not(_, _) => exprs.mk_not(deriv[0]),
                    Expr::Repeat(_, e, min, max) => {
                        if deriv[0] == ExprRef::NO_MATCH {
                            return ExprRef::NO_MATCH;
                        }
                        let max = if max == u32::MAX {
                            u32::MAX
                        } else {
                            max.saturating_sub(1)
                        };
                        let tail = exprs.mk_repeat(e, min.saturating_sub(1), max);
                        exprs.mk_concat(deriv[0], tail)
                    }
                    Expr::Concat(_, [a, b]) => {
                        let d0 = exprs.mk_concat(deriv[0], b);
                        if exprs.is_nullable(a) {
                            or_branches.clear();
                            or_branches.push(d0);
                            or_branches.push(deriv[1]);
                            exprs.mk_or(&mut or_branches)
                        } else {
                            d0
                        }
                    }
                    Expr::Lookahead(_, e, offset) => {
                        if e == ExprRef::EMPTY_STRING {
                            ExprRef::NO_MATCH
                        } else {
                            exprs.mk_lookahead(deriv[0], offset + 1)
                        }
                    }
                };
                debug!(
                    "deriv({}) via {} = {}",
                    exprs.expr_to_string(r),
                    exprs.pp().byte_to_string(b),
                    exprs.expr_to_string(d)
                );
                d
            },
        )
    }

    /// Estimate the size of the regex tables in bytes.
    pub fn num_bytes(&self) -> usize {
        self.state_table.len() * 8 * std::mem::size_of::<isize>()
    }
}
