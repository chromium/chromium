use crate::{FmtArg, PanicFmt, PanicVal, StdWrapper};

use core::ops::{Range, RangeFrom, RangeFull, RangeInclusive, RangeTo, RangeToInclusive};

macro_rules! impl_range_panicfmt_one {
    (
        fn(&$self:ident: $ty:ty, $f:ident) -> $pv_count:literal {
            $($content:tt)*
        }
    ) => {
        impl PanicFmt for $ty {
            type This = Self;
            type Kind = crate::fmt::IsStdType;
            const PV_COUNT: usize = $pv_count;
        }

        impl crate::StdWrapper<&$ty> {
            #[doc = concat!(
                "Converts this `", stringify!($ty), "` to a single-element `PanicVal` array."
            )]
            pub const fn to_panicvals($self, $f: FmtArg) -> [PanicVal<'static>; $pv_count] {
                $($content)*
            }
        }
    }
}

macro_rules! impl_range_panicfmt {
    ($elem_ty:ty) => {
        impl_range_panicfmt_one! {
            fn(&self: Range<$elem_ty>, f) -> 3 {
                [
                    StdWrapper(&self.0.start).to_panicval(f),
                    PanicVal::write_str(".."),
                    StdWrapper(&self.0.end).to_panicval(f),
                ]
            }
        }

        impl_range_panicfmt_one! {
            fn(&self: RangeFrom<$elem_ty>, f) -> 2 {
                [
                    StdWrapper(&self.0.start).to_panicval(f),
                    PanicVal::write_str(".."),
                ]
            }
        }

        impl_range_panicfmt_one! {
            fn(&self: RangeTo<$elem_ty>, f) -> 2 {
                [
                    PanicVal::write_str(".."),
                    StdWrapper(&self.0.end).to_panicval(f),
                ]
            }
        }

        impl_range_panicfmt_one! {
            fn(&self: RangeToInclusive<$elem_ty>, f) -> 2 {
                [
                    PanicVal::write_str("..="),
                    StdWrapper(&self.0.end).to_panicval(f),
                ]
            }
        }

        impl_range_panicfmt_one! {
            fn(&self: RangeInclusive<$elem_ty>, f) -> 3 {
                [
                    StdWrapper(self.0.start()).to_panicval(f),
                    PanicVal::write_str("..="),
                    StdWrapper(self.0.end()).to_panicval(f),
                ]
            }
        }
    };
}

impl_range_panicfmt! {usize}

////////////////////////////////////////////////////////////////////////////////

impl_range_panicfmt_one! {
    fn(&self: RangeFull, _f) -> 1 {
        [PanicVal::write_str("..")]
    }
}
