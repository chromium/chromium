use core::cmp::Ordering;

#[derive(Debug, Copy, Clone, PartialEq, Eq)]
pub struct U8Ordering(pub u8);

impl U8Ordering {
    pub const LESS: Self = Self(0);
    pub const GREATER: Self = Self(1);
    pub const EQUAL: Self = Self(2);

    //#[inline]
    pub const fn to_ordering(self) -> Ordering {
        match self {
            Self::LESS => Ordering::Less,
            Self::GREATER => Ordering::Greater,
            _ => Ordering::Equal,
        }
    }

    //#[inline]
    pub const fn from_ordering(n: Ordering) -> Self {
        Self((n as u8) + 1)
    }
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_ret_if_ne {
    ($left:expr, $right:expr) => {{
        let l = $left;
        let r = $right;
        if l != r {
            return $crate::__::U8Ordering((l > r) as u8);
        }
    }};
}

#[doc(hidden)]
#[macro_export]
macro_rules! __priv_first_expr {
    ($expr:expr) => {
        $expr
    };
    ($expr:expr, $($more:expr),*) => {
        $expr
    };
}
