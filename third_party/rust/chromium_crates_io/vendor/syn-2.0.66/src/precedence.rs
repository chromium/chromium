#[cfg(feature = "printing")]
use crate::expr::Expr;
use crate::op::BinOp;
#[cfg(all(feature = "printing", feature = "full"))]
use crate::ty::ReturnType;
use std::cmp::Ordering;

// Reference: https://doc.rust-lang.org/reference/expressions.html#expression-precedence
pub(crate) enum Precedence {
    // return, break, closures
    Any,
    // = += -= *= /= %= &= |= ^= <<= >>=
    Assign,
    // .. ..=
    Range,
    // ||
    Or,
    // &&
    And,
    // == != < > <= >=
    Compare,
    // |
    BitOr,
    // ^
    BitXor,
    // &
    BitAnd,
    // << >>
    Shift,
    // + -
    Arithmetic,
    // * / %
    Term,
    // as
    Cast,
    // unary - * ! & &mut
    #[cfg(feature = "printing")]
    Prefix,
    // function calls, array indexing, field expressions, method calls, ?
    #[cfg(feature = "printing")]
    Postfix,
    // paths, loops
    #[cfg(feature = "printing")]
    Unambiguous,
}

impl Precedence {
    pub(crate) fn of_binop(op: &BinOp) -> Self {
        match op {
            BinOp::Add(_) | BinOp::Sub(_) => Precedence::Arithmetic,
            BinOp::Mul(_) | BinOp::Div(_) | BinOp::Rem(_) => Precedence::Term,
            BinOp::And(_) => Precedence::And,
            BinOp::Or(_) => Precedence::Or,
            BinOp::BitXor(_) => Precedence::BitXor,
            BinOp::BitAnd(_) => Precedence::BitAnd,
            BinOp::BitOr(_) => Precedence::BitOr,
            BinOp::Shl(_) | BinOp::Shr(_) => Precedence::Shift,

            BinOp::Eq(_)
            | BinOp::Lt(_)
            | BinOp::Le(_)
            | BinOp::Ne(_)
            | BinOp::Ge(_)
            | BinOp::Gt(_) => Precedence::Compare,

            BinOp::AddAssign(_)
            | BinOp::SubAssign(_)
            | BinOp::MulAssign(_)
            | BinOp::DivAssign(_)
            | BinOp::RemAssign(_)
            | BinOp::BitXorAssign(_)
            | BinOp::BitAndAssign(_)
            | BinOp::BitOrAssign(_)
            | BinOp::ShlAssign(_)
            | BinOp::ShrAssign(_) => Precedence::Assign,
        }
    }

    #[cfg(feature = "printing")]
    pub(crate) fn of(e: &Expr) -> Self {
        match e {
            #[cfg(feature = "full")]
            Expr::Closure(e) => match e.output {
                ReturnType::Default => Precedence::Any,
                ReturnType::Type(..) => Precedence::Unambiguous,
            },

            Expr::Break(_) | Expr::Return(_) | Expr::Yield(_) => Precedence::Any,
            Expr::Assign(_) => Precedence::Assign,
            Expr::Range(_) => Precedence::Range,
            Expr::Binary(e) => Precedence::of_binop(&e.op),
            Expr::Cast(_) => Precedence::Cast,
            Expr::Let(_) | Expr::Reference(_) | Expr::Unary(_) => Precedence::Prefix,

            Expr::Await(_)
            | Expr::Call(_)
            | Expr::MethodCall(_)
            | Expr::Field(_)
            | Expr::Index(_)
            | Expr::Try(_) => Precedence::Postfix,

            Expr::Array(_)
            | Expr::Async(_)
            | Expr::Block(_)
            | Expr::Const(_)
            | Expr::Continue(_)
            | Expr::ForLoop(_)
            | Expr::Group(_)
            | Expr::If(_)
            | Expr::Infer(_)
            | Expr::Lit(_)
            | Expr::Loop(_)
            | Expr::Macro(_)
            | Expr::Match(_)
            | Expr::Paren(_)
            | Expr::Path(_)
            | Expr::Repeat(_)
            | Expr::Struct(_)
            | Expr::TryBlock(_)
            | Expr::Tuple(_)
            | Expr::Unsafe(_)
            | Expr::Verbatim(_)
            | Expr::While(_) => Precedence::Unambiguous,

            #[cfg(not(feature = "full"))]
            Expr::Closure(_) => unreachable!(),
        }
    }

    #[cfg(feature = "printing")]
    pub(crate) fn of_rhs(e: &Expr) -> Self {
        match e {
            Expr::Break(_) | Expr::Closure(_) | Expr::Return(_) | Expr::Yield(_) => {
                Precedence::Prefix
            }
            #[cfg(feature = "full")]
            Expr::Range(e) if e.start.is_none() => Precedence::Prefix,
            _ => Precedence::of(e),
        }
    }
}

impl Copy for Precedence {}

impl Clone for Precedence {
    fn clone(&self) -> Self {
        *self
    }
}

impl PartialEq for Precedence {
    fn eq(&self, other: &Self) -> bool {
        *self as u8 == *other as u8
    }
}

impl PartialOrd for Precedence {
    fn partial_cmp(&self, other: &Self) -> Option<Ordering> {
        let this = *self as u8;
        let other = *other as u8;
        Some(this.cmp(&other))
    }
}
