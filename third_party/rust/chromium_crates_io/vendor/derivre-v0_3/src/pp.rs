use std::fmt::Write as _;

use crate::HashMap;

use crate::{
    ast::{byteset_256, byteset_contains, byteset_set, Expr, ExprSet},
    simplify::ConcatElement,
    ExprRef,
};

#[derive(Clone)]
pub struct PrettyPrinter {
    alphabet_mapping: Vec<u8>,
    alphabet_size: usize,
    has_mapping: bool,
}

impl PrettyPrinter {
    pub fn expr_to_string(&self, exprset: &ExprSet, id: ExprRef, max_len: usize) -> String {
        let mut s = String::new(); // format!("|{}| ", exprset.get_weight(id));
        self.write_expr(exprset, id, &mut s, max_len).unwrap();
        s
    }

    pub fn byte_to_string(&self, b: u8) -> String {
        if self.has_mapping {
            symbol_to_string(b)
        } else {
            byte_to_string(b)
        }
    }

    pub fn byteset_to_string(&self, s: &[u32]) -> String {
        if self.has_mapping {
            symbolset_to_string(s, self.alphabet_size)
        } else {
            byteset_to_string(s)
        }
    }

    pub fn new_simple(alphabet_size: usize) -> Self {
        PrettyPrinter {
            alphabet_mapping: (0..=(alphabet_size - 1) as u8).collect(),
            alphabet_size,
            has_mapping: false,
        }
    }

    pub fn new(alphabet_mapping: Vec<u8>, alphabet_size: usize) -> Self {
        let has_mapping = alphabet_size < 256
            || !alphabet_mapping
                .iter()
                .enumerate()
                .all(|(i, &v)| i == v as usize);
        PrettyPrinter {
            alphabet_mapping,
            alphabet_size,
            has_mapping,
        }
    }

    #[allow(dead_code)]
    pub fn alphabet_info(&self) -> String {
        if !self.has_mapping {
            return "".to_string();
        }

        let mut bytes_by_alpha_id = HashMap::default();
        for (b, &alpha_id) in self.alphabet_mapping.iter().enumerate() {
            bytes_by_alpha_id
                .entry(alpha_id)
                .or_insert_with(Vec::new)
                .push(b as u8);
        }

        let mut r = "\n".to_string();

        for alpha_id in 0..self.alphabet_size {
            r.push_str(&format!("    {}: ", symbol_to_string(alpha_id as u8)));
            if let Some(bytes) = bytes_by_alpha_id.get(&(alpha_id as u8)) {
                if bytes.len() == 1 {
                    r.push_str(&byte_to_string(bytes[0]));
                } else {
                    let mut byteset = byteset_256();
                    for b in bytes {
                        byteset_set(&mut byteset, *b as usize);
                    }
                    r.push_str(&byteset_to_string(&byteset));
                }
            } else {
                r.push_str("???");
            }
            r.push('\n');
        }

        r
    }

    fn write_concat(
        &self,
        exprset: &ExprSet,
        concat_expr: ExprRef,
        f: &mut String,
        max_len: usize,
    ) -> std::fmt::Result {
        write!(f, "(")?;
        for (i, elt) in exprset.iter_concat(concat_expr).enumerate() {
            if f.len() > max_len {
                write!(f, "…")?;
                break;
            }

            if i > 0 {
                write!(f, " ")?;
            }

            match elt {
                ConcatElement::Bytes(bytes) => {
                    if let Ok(s) = String::from_utf8(bytes.to_vec()) {
                        if f.len() + s.len() > max_len {
                            write!(f, "…")?;
                            break;
                        } else {
                            write!(f, "{:?}", s)?;
                        }
                    } else {
                        write!(
                            f,
                            "{}",
                            bytes
                                .iter()
                                .map(|b| self.byte_to_string(*b))
                                .collect::<Vec<_>>()
                                .join(" ")
                        )?;
                    }
                }
                ConcatElement::Expr(id) => self.write_expr(exprset, id, f, max_len)?,
            }
        }
        write!(f, ")")
    }

    fn write_exprs(
        &self,
        exprset: &ExprSet,
        sep: &str,
        ids: &[ExprRef],
        f: &mut String,
        max_len: usize,
    ) -> std::fmt::Result {
        write!(f, "(")?;
        for (i, id) in ids.iter().enumerate() {
            if f.len() > max_len {
                write!(f, "…")?;
                break;
            }
            if i > 0 {
                write!(f, "{}", sep)?;
            }
            self.write_expr(exprset, *id, f, max_len)?;
        }
        write!(f, ")")
    }

    fn write_expr(
        &self,
        exprset: &ExprSet,
        id: ExprRef,
        f: &mut String,
        max_len: usize,
    ) -> std::fmt::Result {
        let e = exprset.get(id);
        // if exprset.is_nullable(id) {
        //     write!(f, "@")?;
        // } else if exprset.is_positive(id) {
        //     write!(f, "%")?;
        // }
        // these two gets huge expressions otherwise due to UTF8
        if id == exprset.any_unicode {
            return write!(f, "_");
        }
        if id == exprset.any_unicode_non_nl {
            return write!(f, ".");
        }
        match e {
            Expr::EmptyString => write!(f, "ε"),
            Expr::NoMatch => write!(f, "∅"),
            Expr::Byte(b) => write!(f, "{}", self.byte_to_string(b)),
            Expr::ByteSet(s) => write!(f, "[{}]", self.byteset_to_string(s)),
            Expr::Lookahead(_, e, offset) => {
                write!(f, "(?[+{offset}]=")?;
                self.write_exprs(exprset, "", &[e], f, max_len)?;
                write!(f, ")")
            }
            Expr::Not(_, e) => {
                write!(f, "(¬")?;
                self.write_exprs(exprset, "", &[e], f, max_len)?;
                write!(f, ")")
            }
            Expr::Repeat(_, e, min, max) => {
                self.write_exprs(exprset, "", &[e], f, max_len)?;
                if min == 0 && max == u32::MAX {
                    write!(f, "*")
                } else if min == 1 && max == u32::MAX {
                    write!(f, "+")
                } else if min == 0 && max == 1 {
                    write!(f, "?")
                } else {
                    write!(f, "{{{}, {}}}", min, max)
                }
            }
            Expr::RemainderIs {
                divisor, remainder, ..
            } => {
                write!(f, "( % {} == {} )", divisor, remainder)
            }
            Expr::ByteConcat(_, _, _) | Expr::Concat(_, _) => {
                self.write_concat(exprset, id, f, max_len)
            }
            Expr::Or(_, es) => self.write_exprs(exprset, " | ", es, f, max_len),
            Expr::And(_, es) => self.write_exprs(exprset, " & ", es, f, max_len),
        }
    }
}

pub fn symbol_to_string(b: u8) -> String {
    format!("s{}", b)
}

pub fn symbolset_to_string(s: &[u32], alpha_size: usize) -> String {
    let mut res = String::new();
    let mut start = None;
    let mut first = true;
    let mut num_set = 0;
    for i in 0..=alpha_size {
        if i < alpha_size && byteset_contains(s, i) {
            num_set += 1;
            if start.is_none() {
                start = Some(i);
            }
        } else {
            if let Some(start) = start {
                if !first {
                    res.push(';');
                }
                first = false;
                res.push_str(&symbol_to_string(start as u8));
                if i - start > 1 {
                    res.push('-');
                    res.push_str(&symbol_to_string((i - 1) as u8));
                }
            }
            start = None;
        }
    }
    if num_set == alpha_size {
        res = "_".to_string();
    }
    res
}

pub fn byte_to_string(b: u8) -> String {
    if !(0x20..0x7f).contains(&b) {
        format!("{:02X}", b)
    } else {
        format!("{:?}", b as char)
    }
}

pub fn byteset_to_string(s: &[u32]) -> String {
    let mut res = String::new();
    let mut start = None;
    let mut first = true;
    for i in 0..=256 {
        if i <= 0xff && byteset_contains(s, i) {
            if start.is_none() {
                start = Some(i);
            }
        } else {
            if let Some(start) = start {
                if !first {
                    res.push(';');
                }
                first = false;
                res.push_str(&byte_to_string(start as u8));
                if i - start > 1 {
                    res.push('-');
                    res.push_str(&byte_to_string((i - 1) as u8));
                }
            }
            start = None;
        }
    }
    res
}
