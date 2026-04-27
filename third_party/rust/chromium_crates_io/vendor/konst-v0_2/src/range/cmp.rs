use core::ops::{Range, RangeInclusive};

use crate::polymorphism::{CmpWrapper, ConstCmpMarker, IsAConstCmpMarker, IsStdKind};

macro_rules! shared_range_impls {
    ($type:ty, $eq_fn_name:ident) => {
        impl ConstCmpMarker for $type {
            type Kind = IsStdKind;
            type This = Self;
        }
        impl CmpWrapper<$type> {
            /// Compares `self` and `other` for equality.
            #[inline(always)]
            pub const fn const_eq(&self, other: &$type) -> bool {
                $eq_fn_name(&self.0, other)
            }
        }
    };
}

macro_rules! declare_range_cmp_fns {
    (
        ($type:ty, ($eq_fn_name:ident))

        docs( $docs_eq:expr, $docs_cmp:expr,)
    ) => {
        shared_range_impls! {$type, $eq_fn_name}

        impl<T> IsAConstCmpMarker<IsStdKind, $type, T> {
            ///
            #[inline(always)]
            pub const fn coerce(self, range: &$type) -> CmpWrapper<$type> {
                CmpWrapper(Range {
                    start: range.start,
                    end: range.end,
                })
            }
        }

        __delegate_const_eq! {
            skip_coerce;
            for['a,]

            #[doc = $docs_eq]
            pub const fn $eq_fn_name(ref left: &'a $type, right: &'a $type) -> bool {
                left.start == right.start && left.end == right.end
            }
        }
    };
}

__declare_fns_with_docs! {
    (Range<u8>,    (eq_range_u8))
    (Range<u16>,   (eq_range_u16))
    (Range<u32>,   (eq_range_u32))
    (Range<u64>,   (eq_range_u64))
    (Range<u128>,  (eq_range_u128))
    (Range<usize>, (eq_range_usize))

    (Range<char>, (eq_range_char))

    docs(default)

    macro = declare_range_cmp_fns!(),
}

macro_rules! declare_rangeinclusive_cmp_fns {
    (
        ($type:ty, ($eq_fn_name:ident))

        docs( $docs_eq:expr, $docs_cmp:expr,)
    ) => {
        shared_range_impls! {$type, $eq_fn_name}

        impl<T> IsAConstCmpMarker<IsStdKind, $type, T> {
            ///
            #[inline(always)]
            pub const fn coerce(self, range: &$type) -> CmpWrapper<$type> {
                CmpWrapper(RangeInclusive::new(*range.start(), *range.end()))
            }
        }

        #[doc = $docs_eq]
        pub const fn $eq_fn_name(left: &$type, right: &$type) -> bool {
            *left.start() == *right.start() && *left.end() == *right.end()
        }
    };
}

__declare_fns_with_docs! {
    (RangeInclusive<u8>,    (eq_rangeinc_u8,))
    (RangeInclusive<u16>,   (eq_rangeinc_u16))
    (RangeInclusive<u32>,   (eq_rangeinc_u32))
    (RangeInclusive<u64>,   (eq_rangeinc_u64))
    (RangeInclusive<u128>,  (eq_rangeinc_u128))
    (RangeInclusive<usize>, (eq_rangeinc_usize))

    (RangeInclusive<char>, (eq_rangeinc_char))

    docs(default)

    macro = declare_rangeinclusive_cmp_fns!(),
}
