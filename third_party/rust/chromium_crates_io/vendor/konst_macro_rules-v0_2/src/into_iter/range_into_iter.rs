use core::{
    mem::ManuallyDrop,
    ops::{Range, RangeFrom, RangeInclusive},
};

use super::{IntoIterKind, IntoIterWrapper, IsIteratorKind, IsStdKind};

macro_rules! impl_std_kinds {
    ($($ty:ident),*) => (
        $(
            impl<T> IntoIterKind for $ty<T> {
                type Kind = IsStdKind;
            }
            impl<T> IntoIterKind for &$ty<T> {
                type Kind = IsStdKind;
            }
        )*
    )
}
impl_std_kinds! {Range, RangeInclusive, RangeFrom}

pub struct RangeIter<T> {
    start: T,
    end: T,
}
impl<T> IntoIterKind for RangeIter<T> {
    type Kind = IsIteratorKind;
}

pub struct RangeIterRev<T> {
    start: T,
    end: T,
}
impl<T> IntoIterKind for RangeIterRev<T> {
    type Kind = IsIteratorKind;
}

pub struct RangeInclusiveIter<T> {
    start: T,
    end: T,
}
impl<T> IntoIterKind for RangeInclusiveIter<T> {
    type Kind = IsIteratorKind;
}

pub struct RangeInclusiveIterRev<T> {
    start: T,
    end: T,
}
impl<T> IntoIterKind for RangeInclusiveIterRev<T> {
    type Kind = IsIteratorKind;
}

pub struct RangeFromIter<T> {
    start: T,
}
impl<T> IntoIterKind for RangeFromIter<T> {
    type Kind = IsIteratorKind;
}

macro_rules! int_range_shared {
    (is_forward = $is_forward:ident, int = $Int:ty) => {
        iterator_shared! {
            is_forward = $is_forward,
            item = $Int,
            iter_forward = RangeIter<$Int>,
            iter_reversed = RangeIterRev<$Int>,
            next(self){
                if self.start >= self.end {
                    None
                } else {
                    let ret = self.start;
                    self.start += 1;
                    Some((ret, self))
                }
            },
            next_back {
                if self.start >= self.end {
                    None
                } else {
                    self.end -= 1;
                    Some((self.end, self))
                }
            },
            fields = {start, end},
        }
    };
}

macro_rules! range_exc_impls {
    ($($ty:ty),*) => (
        $(
            impl RangeIter<$ty> {
                int_range_shared!{is_forward = true, int = $ty}
            }

            impl RangeIterRev<$ty> {
                int_range_shared!{is_forward = false, int = $ty}
            }
        )*
    )
}

//////////////////////////////////////////////////

macro_rules! int_range_inc_shared {
    (is_forward = $is_forward:ident, int = $Int:ty) => {
        iterator_shared! {
            is_forward = $is_forward,
            item = $Int,
            iter_forward = RangeInclusiveIter<$Int>,
            iter_reversed = RangeInclusiveIterRev<$Int>,
            next(self){
                if self.start > self.end {
                    None
                } else {
                    let ret = self.start;
                    if self.start == self.end {
                        self.end = 0;
                        self.start = 1;
                    } else {
                        self.start += 1;
                    }
                    Some((ret, self))
                }
            },
            next_back {
                if self.start > self.end {
                    None
                } else {
                    let ret;
                    if self.start == self.end {
                        ret = self.end;
                        self.end = 0;
                        self.start = 1;
                    } else {
                        ret = self.end;
                        self.end -= 1;
                    }
                    Some((ret, self))
                }
            },
            fields = {start, end},
        }
    };
}

macro_rules! range_inc_impls {
    ($($ty:ty),*) => (
        $(
            impl RangeInclusiveIter<$ty> {
                int_range_inc_shared!{is_forward = true, int = $ty}
            }

            impl RangeInclusiveIterRev<$ty> {
                int_range_inc_shared!{is_forward = false, int = $ty}
            }
        )*
    )
}

////////////////////////////////////////////////////////////////////////////////////////////////////

macro_rules! int_range_from_shared {
    (int = $Int:ty) => {
        iterator_shared! {
            is_forward = true,
            item = $Int,
            iter_forward = RangeFromIter<$Int>,
            next(self){
                let ret = self.start;
                self.start += 1;
                Some((ret, self))
            },
            fields = {start},
        }
    };
}

macro_rules! range_from_impls {
    ($($ty:ty),*) => (
        $(
            impl RangeFromIter<$ty> {
                int_range_from_shared!{int = $ty}
            }
        )*
    )
}

//////////////////////////////////////////////////

macro_rules! ii_wrapper_range_impls {
    ($Int:ty, $($reff:tt)?) => {
        impl IntoIterWrapper<$($reff)? Range<$Int>, IsStdKind> {
            pub const fn const_into_iter(self) -> RangeIter<$Int> {
                let range = ManuallyDrop::into_inner(self.iter);
                RangeIter {
                    start: range.start,
                    end: range.end,
                }
            }
        }

        impl IntoIterWrapper<$($reff)? RangeInclusive<$Int>, IsStdKind> {
            pub const fn const_into_iter(self) -> RangeInclusiveIter<$Int> {
                let range = ManuallyDrop::into_inner(self.iter);
                RangeInclusiveIter {
                    start: *range.start(),
                    end: *range.end(),
                }
            }
        }

        impl IntoIterWrapper<$($reff)? RangeFrom<$Int>, IsStdKind> {
            pub const fn const_into_iter(self) -> RangeFromIter<$Int> {
                let range = ManuallyDrop::into_inner(self.iter);
                RangeFromIter {
                    start: range.start,
                }
            }
        }

    }
}

macro_rules! all_range_impls {
    ($($Int:ty),*) => (

        $(
            ii_wrapper_range_impls!{$Int, }
            ii_wrapper_range_impls!{$Int, &}
        )*

        range_exc_impls!{$($Int),*}

        range_inc_impls!{$($Int),*}

        range_from_impls!{$($Int),*}
    )
}

all_range_impls! {usize}
