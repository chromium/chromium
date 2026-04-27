use super::{IntoIterKind, IntoIterWrapper, IsIteratorKind, IsStdKind};

use core::mem::ManuallyDrop;

#[cfg(feature = "rust_1_51")]
macro_rules! array_impls {
    ($($tt:tt)*) => {
        impl<T, const N: usize> IntoIterKind for &[T; N] {
            type Kind = IsStdKind;
        }

        impl<T, const N: usize> IntoIterKind for &&[T; N] {
            type Kind = IsStdKind;
        }

        impl<'a, T, const N: usize> IntoIterWrapper<&'a [T; N], IsStdKind> {
            pub const fn const_into_iter(self) -> Iter<'a, T> {
                Iter {
                    slice: ManuallyDrop::into_inner(self.iter) as &[T],
                }
            }
        }
        impl<'a, T, const N: usize> IntoIterWrapper<&&'a [T; N], IsStdKind> {
            pub const fn const_into_iter(self) -> Iter<'a, T> {
                Iter {
                    slice: (*ManuallyDrop::into_inner(self.iter)) as &[T],
                }
            }
        }
    };
}

#[cfg(not(feature = "rust_1_51"))]
macro_rules! array_impls {
    ($($len:literal),* $(,)* ) => (
        $(
            impl<T> IntoIterKind for &[T; $len] {
                type Kind = IsStdKind;
            }
            impl<T> IntoIterKind for &&[T; $len] {
                type Kind = IsStdKind;
            }

            impl<'a, T> IntoIterWrapper<&'a [T; $len], IsStdKind> {
                pub const fn const_into_iter(self) -> Iter<'a, T> {
                    Iter { slice: ManuallyDrop::into_inner(self.iter) as &[T] }
                }
            }
            impl<'a, T> IntoIterWrapper<&&'a [T; $len], IsStdKind> {
                pub const fn const_into_iter(self) -> Iter<'a, T> {
                    Iter { slice: (*ManuallyDrop::into_inner(self.iter)) as &[T] }
                }
            }
        )*
    )
}

array_impls! {
    0,1,2,3,4,5,6,7,8,9,10,11,12,13,14,15,
    16,17,18,19,20,21,22,23,24,25,26,27,28,29,30,31,
    32,
}

impl<T> IntoIterKind for &[T] {
    type Kind = IsStdKind;
}

impl<'a, T> IntoIterWrapper<&'a [T], IsStdKind> {
    pub const fn const_into_iter(self) -> Iter<'a, T> {
        Iter {
            slice: ManuallyDrop::into_inner(self.iter),
        }
    }
}

impl<T> IntoIterKind for &&[T] {
    type Kind = IsStdKind;
}

impl<'a, T> IntoIterWrapper<&&'a [T], IsStdKind> {
    pub const fn const_into_iter(self) -> Iter<'a, T> {
        Iter {
            slice: *ManuallyDrop::into_inner(self.iter),
        }
    }
}

pub const fn iter<T>(slice: &[T]) -> Iter<'_, T> {
    Iter { slice }
}

macro_rules! iter_shared {
    (is_forward = $is_forward:ident) => {
        iterator_shared! {
            is_forward = $is_forward,
            item = &'a T,
            iter_forward = Iter<'a, T>,
            iter_reversed = IterRev<'a, T>,
            next(self) {
                if let [elem, rem @ ..] = self.slice {
                    self.slice = rem;
                    Some((elem, self))
                } else {
                    None
                }
            },
            next_back {
                if let [rem @ .., elem] = self.slice {
                    self.slice = rem;
                    Some((elem, self))
                } else {
                    None
                }
            },
            fields = {slice},
        }

        /// Accesses the remaining slice.
        pub const fn as_slice(&self) -> &'a [T] {
            self.slice
        }
    };
}

pub struct Iter<'a, T> {
    slice: &'a [T],
}
impl<'a, T> IntoIterKind for Iter<'a, T> {
    type Kind = IsIteratorKind;
}

pub struct IterRev<'a, T> {
    slice: &'a [T],
}
impl<'a, T> IntoIterKind for IterRev<'a, T> {
    type Kind = IsIteratorKind;
}

impl<'a, T> Iter<'a, T> {
    iter_shared! {is_forward = true}
}

impl<'a, T> IterRev<'a, T> {
    iter_shared! {is_forward = false}
}

#[cfg(feature = "rust_1_61")]
pub use copied::{iter_copied, IterCopied, IterCopiedRev};

#[cfg(feature = "rust_1_61")]
mod copied {
    use super::*;

    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_61")))]
    pub const fn iter_copied<T: Copy>(slice: &[T]) -> IterCopied<'_, T> {
        IterCopied { slice }
    }

    macro_rules! iter_copied_shared {
        (is_forward = $is_forward:ident) => {
            iterator_shared! {
                is_forward = $is_forward,
                item = T,
                iter_forward = IterCopied<'a, T>,
                iter_reversed = IterCopiedRev<'a, T>,
                next(self) {
                    if let [elem, rem @ ..] = self.slice {
                        self.slice = rem;
                        Some((*elem, self))
                    } else {
                        None
                    }
                },
                next_back {
                    if let [rem @ .., elem] = self.slice {
                        self.slice = rem;
                        Some((*elem, self))
                    } else {
                        None
                    }
                },
                fields = {slice},
            }

            /// Accesses the remaining slice.
            pub const fn as_slice(&self) -> &'a [T] {
                self.slice
            }
        };
    }

    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_61")))]
    pub struct IterCopied<'a, T> {
        slice: &'a [T],
    }
    impl<'a, T> IntoIterKind for IterCopied<'a, T> {
        type Kind = IsIteratorKind;
    }

    #[cfg_attr(feature = "docsrs", doc(cfg(feature = "rust_1_61")))]
    pub struct IterCopiedRev<'a, T> {
        slice: &'a [T],
    }
    impl<'a, T> IntoIterKind for IterCopiedRev<'a, T> {
        type Kind = IsIteratorKind;
    }

    impl<'a, T: Copy> IterCopied<'a, T> {
        iter_copied_shared! {is_forward = true}
    }

    impl<'a, T: Copy> IterCopiedRev<'a, T> {
        iter_copied_shared! {is_forward = false}
    }
}
