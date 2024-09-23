#[cfg(feature = "printing")]
use crate::expr::Expr;
#[cfg(all(feature = "printing", feature = "full"))]
use crate::expr::{ExprBreak, ExprReturn, ExprYield};
use crate::op::BinOp;
#[cfg(all(feature = "printing", feature = "full"))]
use crate::ty::ReturnType;
use std::cmp::Ordering;

// Reference: https://doc.rust-lang.org/reference/expressions.html#expression-precedence
pub(crate) enum Precedence {
    // return, break, closures
    Jump,
    // = += -= *= /= %= &= |= ^= <<= >>=
    Assign,
    // .. ..=
    Range,
    // ||
    Or,
    // &&
    And,
    // let
    #[cfg(feature = "printing")]
    Let,
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
    Sum,
    // * / %
    Product,
    // as
    Cast,
    // unary - * ! & &mut
    #[cfg(feature = "printing")]
    Prefix,
    // paths, loops, function calls, array indexing, field expressions, method calls
    #[cfg(feature = "printing")]
    Unambiguous,
}

impl Precedence {
    pub(crate) const MIN: Self = Precedence::Jump;

    pub(crate) fn of_binop(op: &BinOp) -> Self {
        match op {
            BinOp::Add(_) | BinOp::Sub(_) => Precedence::Sum,
            BinOp::Mul(_) | BinOp::Div(_) | BinOp::Rem(_) => Precedence::Product,
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
                ReturnType::Default => Precedence::Jump,
                ReturnType::Type(..) => Precedence::Unambiguous,
            },

            #[cfg(feature = "full")]
            Expr::Break(ExprBreak { expr, .. })
            | Expr::Return(ExprReturn { expr, .. })
            | Expr::Yield(ExprYield { expr, .. }) => match expr {
                Some(_) => Precedence::Jump,
                None => Precedence::Unambiguous,
            },

            Expr::Assign(_) => Precedence::Assign,
            Expr::Range(_) => Precedence::Range,
            Expr::Binary(e) => Precedence::of_binop(&e.op),
            Expr::Let(_) => Precedence::Let,
            Expr::Cast(_) => Precedence::Cast,
            Expr::Reference(_) | Expr::Unary(_) => Precedence::Prefix,

            Expr::Array(_)
            | Expr::Async(_)
            | Expr::Await(_)
            | Expr::Block(_)
            | Expr::Call(_)
            | Expr::Const(_)
            | Expr::Continue(_)
            | Expr::Field(_)
            | Expr::ForLoop(_)
            | Expr::Group(_)
            | Expr::If(_)
            | Expr::Index(_)
            | Expr::Infer(_)
            | Expr::Lit(_)
            | Expr::Loop(_)
            | Expr::Macro(_)
            | Expr::Match(_)
            | Expr::MethodCall(_)
            | Expr::Paren(_)
            | Expr::Path(_)
            | Expr::Repeat(_)
            | Expr::Struct(_)
            | Expr::Try(_)
            | Expr::TryBlock(_)
            | Expr::Tuple(_)
            | Expr::Unsafe(_)
            | Expr::Verbatim(_)
            | Expr::While(_) => Precedence::Unambiguous,

            #[cfg(not(feature = "full"))]
            Expr::Break(_) | Expr::Closure(_) | Expr::Return(_) | Expr::Yield(_) => unreachable!(),
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
