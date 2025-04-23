use crate::HashMap;

use crate::ast::{Expr, ExprRef, ExprSet, NextByte};

#[derive(Clone)]
pub struct NextByteCache {
    next_byte_cache: HashMap<ExprRef, NextByte>,
}

pub(crate) fn next_byte_simple(exprs: &ExprSet, mut r: ExprRef) -> NextByte {
    let mut fuzzy = false;
    let res = 'dfs: loop {
        match exprs.get(r) {
            Expr::EmptyString => break NextByte::ForcedEOI,
            Expr::NoMatch => break NextByte::Dead,
            Expr::ByteSet(lst) => {
                let mut b0 = None;
                for (idx, &w) in lst.iter().enumerate() {
                    if w > 0 {
                        let b = (idx as u32 * 32 + w.trailing_zeros()) as u8;
                        if let Some(b1) = b0 {
                            break 'dfs NextByte::SomeBytes2([b1, b]);
                        } else {
                            b0 = Some(b);
                        }
                        let w = w & !(1 << (b as u32 % 32));
                        if w > 0 {
                            let b2 = (idx as u32 * 32 + w.trailing_zeros()) as u8;
                            break 'dfs NextByte::SomeBytes2([b, b2]);
                        }
                    }
                }
                unreachable!("ByteSet should have at least two bytes set");
            }
            Expr::Byte(b) => break NextByte::ForcedByte(b),
            Expr::ByteConcat(_, bytes, _) => break NextByte::ForcedByte(bytes[0]),
            Expr::Or(_, args) | Expr::And(_, args) => {
                fuzzy = true;
                r = args[0];
            }
            Expr::Not(_, _) => break NextByte::SomeBytes0,
            Expr::RemainderIs { .. } => {
                break NextByte::SomeBytes2([exprs.digits[0], exprs.digits[1]]);
            }
            Expr::Lookahead(_, e, _) => {
                r = e;
            }
            Expr::Repeat(_, arg, min, _) => {
                if min == 0 {
                    fuzzy = true;
                }
                r = arg;
            }
            Expr::Concat(_, args) => {
                if exprs.is_nullable(args[0]) {
                    fuzzy = true;
                }
                r = args[0];
            }
        }
    };
    if fuzzy {
        res.make_fuzzy()
    } else {
        res
    }
}

impl Default for NextByteCache {
    fn default() -> Self {
        Self::new()
    }
}

impl NextByteCache {
    pub fn new() -> Self {
        NextByteCache {
            next_byte_cache: HashMap::default(),
        }
    }

    pub fn num_bytes(&self) -> usize {
        self.next_byte_cache.len() * 4 * std::mem::size_of::<isize>()
    }

    pub fn next_byte(&mut self, exprs: &ExprSet, r: ExprRef) -> NextByte {
        if let Some(&found) = self.next_byte_cache.get(&r) {
            return found;
        }
        let next = match exprs.get(r) {
            Expr::Or(_, args) => {
                let mut found = next_byte_simple(exprs, args[0]);
                for child in args.iter().skip(1) {
                    if found.is_some_bytes() {
                        break;
                    }
                    found = found | next_byte_simple(exprs, *child);
                }
                found
            }
            _ => next_byte_simple(exprs, r),
        };
        self.next_byte_cache.insert(r, next);
        next
    }
}
